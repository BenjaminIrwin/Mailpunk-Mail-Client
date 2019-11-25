#include "imap.hpp"
#include "imaputils.hpp"
#include <libetpan/libetpan.h>
#include <string>
#include <cstring>
#include <functional>
#include <iostream>
#include <locale>

using namespace IMAP;

Session::Session(std::function<void()> updateUI)
{
	imap = mailimap_new(0, nullptr);
	reloadUI = updateUI;
}

Message** Session::getMessages()
{
	uint32_t uid, num_messages = count_mail();
	clistiter * cur;
	int x = 0;
	
	if(num_messages == 0)//If mailbox empty, return NULL
	{
		set_of_messages = new Message*[1];
		set_of_messages[0] = NULL;
		return set_of_messages;
	}

	set_of_messages = new Message*[num_messages + 1];

	//Prepare fetch
	auto set = mailimap_set_new_interval(1, 0);
	auto fetch_att = mailimap_fetch_att_new_uid();
	auto fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
	check_error(mailimap_fetch_type_new_fetch_att_list_add(fetch_type, 
		fetch_att), "could not add attribute to new fetch structure");
	
	//Fetch messages
	clist * fetch_result;
	check_error(mailimap_fetch(imap, set, fetch_type, &fetch_result), 
					"could not fetch messages");

	for(cur = clist_begin(fetch_result) ; cur != NULL ; x++, 
		cur = clist_next(cur))
	{
		auto msg_att = static_cast<mailimap_msg_att*>clist_content(cur);
		uid = get_UID(msg_att);

		if (uid)
			set_of_messages[x] = new Message(uid, imap, this);
	}

	set_of_messages[x] = NULL;

	//Free memory
	mailimap_fetch_type_free(fetch_type);
	mailimap_fetch_list_free(fetch_result);
	mailimap_set_free(set);

	return set_of_messages;
}

uint32_t Session::get_UID(mailimap_msg_att* msg_att)
{
	clistiter* cur;
	for(cur = clist_begin(msg_att->att_list) ; cur != NULL ; 
		cur = clist_next(cur))
	{
		auto item = static_cast<mailimap_msg_att_item*>clist_content(cur);
		if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC && 
		item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_UID)
			return item->att_data.att_static->att_data.att_uid;

	}
	
	return 0;
}

uint32_t Session::count_mail()
{ 
	uint32_t num_of_messages = 0;
	mailimap_mailbox_data_status* mailbox_info;
	auto status_att_list = mailimap_status_att_list_new_empty();

	check_error(mailimap_status_att_list_add(status_att_list, 
			MAILIMAP_STATUS_ATT_MESSAGES), "could not count mail.");
	check_error(mailimap_status(imap, "INBOX", status_att_list, 
			&mailbox_info), "could not count mail.");

	clist* count_fetch = mailbox_info->st_info_list;
	auto info = static_cast<mailimap_status_info*>clist_content(clist_begin(count_fetch));
	num_of_messages = info->st_value;

	//Free memory
	mailimap_mailbox_data_status_free(mailbox_info);
	mailimap_status_att_list_free(status_att_list);
	
	return num_of_messages;
}

void Session::connect(std::string const& server, size_t port)
{
	check_error(mailimap_socket_connect(imap, server.c_str(), port), 
			"could not connect to server.");
}

void Session::login(std::string const& userid, std::string const& password)
{
	int error = mailimap_login(imap, userid.c_str(), password.c_str());
	check_error(error, "could not log in.");
	if(error == MAILIMAP_NO_ERROR)//If login successful
	{		
		logged_in = true;
	}
}

void Session::selectMailbox(std::string const& mailbox)
{
	check_error(mailimap_select(imap, mailbox.c_str()), 
			"could not select mailbox.");
}

Session::~Session()
{	
	if(logged_in)//Free dynamically allocated message array
	{	
		for(int i = 0; set_of_messages[i] != NULL; i++)
		{
			delete set_of_messages[i];
		}

		delete [] set_of_messages;
		set_of_messages = NULL;
		mailimap_logout(imap);
	}
	
	//Free memory
	mailimap_free(imap);	
}

Message::Message(uint32_t uid, mailimap* imap, Session* session):uid(uid), 
			imap(imap), session(session){};

std::string Message::getBody()
{
	//Prepare fetch
	auto set = mailimap_set_new_single(uid);	
	auto section = mailimap_section_new(NULL);
	auto fetch_att = mailimap_fetch_att_new_body_peek_section(section);
	auto fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
	clist * fetch_result;
	char* msg_content;

	//Fetch body
	check_error(mailimap_fetch_type_new_fetch_att_list_add(fetch_type, 
			fetch_att), "could not fetch message body");
	check_error(mailimap_uid_fetch(imap, set, fetch_type, &fetch_result), 
					"could not fetch message body.");

	msg_content = get_msg_content(fetch_result);

	if(msg_content == NULL)
	{
		//Free memory
		mailimap_fetch_type_free(fetch_type);
		mailimap_fetch_list_free(fetch_result);
		mailimap_set_free(set);
		return "NO BODY";
	}	
	
	body = msg_content;

	//Free memory
	mailimap_fetch_type_free(fetch_type);
	mailimap_fetch_list_free(fetch_result);
	mailimap_set_free(set);

	return body;
}

char* Message::get_msg_content(clist * fetch_result)
{
	clistiter* cur;
	int x = 0;
	
	//Retrieve message content
	for(cur = clist_begin(fetch_result) ; cur != NULL ; x++, 
		cur = clist_next(cur))
	{
		auto msg_att = static_cast<mailimap_msg_att*>clist_content(cur);
		char* msg_content = get_msg_att_msg_content(msg_att);

		if (msg_content != NULL)
			return msg_content;		
	}
	
	return NULL;
}

char* Message::get_msg_att_msg_content(mailimap_msg_att* msg_att)
{
	clistiter* cur;
	
	//Retrieve message att
	for(cur = clist_begin(msg_att->att_list) ; cur != NULL ; 
		cur = clist_next(cur))
	{
		auto item = static_cast<mailimap_msg_att_item*>clist_content(cur);
		
		if(item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC && 
			item->att_data.att_static->att_type == 
				MAILIMAP_MSG_ATT_BODY_SECTION)
		return item->att_data.att_static->att_data.att_body_section->sec_body_part;
	}

	return NULL;
}

std::string Message::getField(std::string fieldname)
{
	clist* fetch_result;
	
	//Prepare fetch	
	auto set = mailimap_set_new_single(uid);
	auto fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
	auto section = mailimap_section_new_header();
	auto fetch_att = mailimap_fetch_att_new_body_section(section);

	//Fetch field
	check_error(mailimap_fetch_type_new_fetch_att_list_add(fetch_type, 
			fetch_att), "could not get " + fieldname + " field.");
	check_error(mailimap_uid_fetch(imap, set, fetch_type, &fetch_result), 
				"could not get " + fieldname + " field.");

	char *header_content = get_msg_content(fetch_result);

	if (header_content == NULL)
	{
		//Free memory
		mailimap_fetch_type_free(fetch_type);
		mailimap_fetch_list_free(fetch_result);
		mailimap_set_free(set);
		return "NO FIELD";
	}

	std::string field = format_field(header_content, fieldname);

	//Free memory
	mailimap_fetch_type_free(fetch_type);
	mailimap_fetch_list_free(fetch_result);
	mailimap_set_free(set);

	return field;
}

std::string Message::format_field(char* header_content, std::string fieldname)
{
	std::string field = header_content;
	
	//Look for field
	if(field.find(fieldname) == std::string::npos)
		return "No " + fieldname;

	size_t pos = field.find(fieldname) + fieldname.length();

	//Erase prepended part of field string
	if(field[pos+1] == ' ')
		field.erase(0, pos + 2);
	else
		field.erase(0, pos + 1);
	
	//Erase appended part of field string
	pos = field.find_first_of('\n', 0);
	field.erase(pos - 1, field.length());

	//Identify if string is now empty
	if(field.find_first_not_of(' ') == std::string::npos)
		return "No " + fieldname;
	
	return field;
}

void Message::deleteFromMailbox()
{
	//In the case that mailbox is already empty
	if(this == NULL)
	{
		delete this;	
		return;
	}

	//Prepare deletion
	auto flag_list = mailimap_flag_list_new_empty();
	auto deleted_flag = mailimap_flag_new_deleted();
	check_error(mailimap_flag_list_add(flag_list, deleted_flag), 
			"could not delete");
	auto store = mailimap_store_att_flags_new_set_flags(flag_list);
	auto set = mailimap_set_new_single(uid);
	check_error(mailimap_uid_store(imap, set, store), "could not delete");
	
	//Delete from server
	check_error(mailimap_expunge(imap), "could not delete");
	
	//Delete message array prior to updating UI
	for(int i = 0; session->set_of_messages[i] != NULL; i++)
	{
		if(!(session->set_of_messages[i]->uid == uid))
			delete session->set_of_messages[i];
	}

	delete [] session->set_of_messages;
	session->set_of_messages = NULL;

	//Free memory
	mailimap_store_att_flags_free(store);
	mailimap_set_free(set);
	session->reloadUI();

	delete this;
}
