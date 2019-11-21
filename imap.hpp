#ifndef IMAP_H
#define IMAP_H
#include "imaputils.hpp"
#include <libetpan/libetpan.h>
#include <string>
#include <functional>

namespace IMAP {

class Message;

class Session {

public:
	/**
	 * Session constructor. 
	 */
	Session(std::function<void()> updateUI);

	/**
	 * Function pointer to be defined in constructor 
	 */
	std::function<void()> reloadUI;
	
	/**
	 * Get all messages in the INBOX mailbox terminated by a nullptr 
	 */
	Message** getMessages();

	/**
	 * connect to the specified server (143 is the standard imap port)
	 */
	void connect(std::string const& server, size_t port = 143);

	/**
	 * log in to the server (connect first, then log in)
	 */
	void login(std::string const& userid, std::string const& password);

	/**
	 * select a mailbox (only one can be selected at any given time)
	 * 
	 * this can only be performed after login
	 */
	void selectMailbox(std::string const& mailbox);
	
	/**
	 * Session destructor. Deletes dynamically allocated message array, 
	 * logs out and frees the imap session. 
	 */
	~Session();

	friend Message;
private:
	Message** set_of_messages;
	mailimap* imap;
	bool logged_in = false;

	/**
	 * Counts the number of messages in the mailbox prior to full execution
	 * of getMessages(). 
	 */
	uint32_t count_mail();
	
	/**
	 * Gets the unique identifier of message.
	 */
	uint32_t get_UID(mailimap_msg_att* msg_att);

};

class Message {

public:
	/**
	 * Message constructor. 
	 */
	Message(uint32_t uid, mailimap* imap, Session* session);
	/**
	 * Get the body of the message. You may chose to either include the 
	 * headers or not.
	 */
	std::string getBody();
	/**
	 * Get one of the descriptor fields (subject, from, ...)
	 */
	std::string getField(std::string fieldname);
	/**
	 * Remove this mail from its mailbox and reload the UI.
	 */
	void deleteFromMailbox();

private:
	uint32_t uid;
	mailimap* imap;
	Session* session;
	std::string body;
	/**
	 * Retrieves body or header for getBody() and getField()
	 */
	char* get_msg_content(clist *fetch_result);
	/**
	 * Retrieves content for get_msg_content
	 */
	char* get_msg_att_msg_content(mailimap_msg_att* msg_att);
	/**
	 * Detects blank field and formats a given header for UI display.
	 */
	std::string format_field(char* header_content, std::string fieldname);

};
}

#endif /* IMAP_H */
