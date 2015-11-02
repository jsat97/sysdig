//
// k8s_dispatcher.h
//
// kubernetes REST API notification abstraction
//

#pragma once

#include "k8s_common.h"
#include "k8s_component.h"
#include "k8s_event_data.h"
#include "json/json.h"
#include <deque>
#include <string>

class k8s_dispatcher
{
public:
	enum msg_reason
	{
		COMPONENT_ADDED,
		COMPONENT_MODIFIED,
		COMPONENT_DELETED,
		COMPONENT_ERROR,
		COMPONENT_UNKNOWN // only to mark bad event messages
	};

	struct msg_data
	{
		msg_reason  m_reason = COMPONENT_UNKNOWN;
		std::string m_name;
		std::string m_uid;
		std::string m_namespace;

		bool is_valid() const
		{
			return m_reason != COMPONENT_UNKNOWN;
		}
	};

	k8s_dispatcher() = delete;
	
	k8s_dispatcher(k8s_component::type t,
		k8s_state_s& state
#ifndef K8S_DISABLE_THREAD
		,std::mutex& mut
#endif
	);

	void enqueue(k8s_event_data&& data);

private:
	const std::string& next_msg();
	
	msg_data get_msg_data(const Json::Value& root);

	bool is_valid(const std::string& msg);

	bool is_ready(const std::string& msg);

	void remove();

	void dispatch();
	
	void handle_node(const Json::Value& root, const msg_data& data);
	void handle_namespace(const Json::Value& root, const msg_data& data);
	void handle_pod(const Json::Value& root, const msg_data& data);
	void handle_rc(const Json::Value& root, const msg_data& data);
	void handle_service(const Json::Value& root, const msg_data& data);

	static std::string to_reason_desc(msg_reason reason);
	
	static msg_reason to_reason(const std::string& desc);

	typedef std::deque<std::string> list;

	k8s_component::type m_type;
	list                m_messages;
	k8s_state_s&        m_state;
#ifndef K8S_DISABLE_THREAD
	std::mutex&         m_mutex;
#endif
};


inline const std::string& k8s_dispatcher::next_msg()
{
	return m_messages.front();
}

inline void k8s_dispatcher::remove()
{
	m_messages.pop_front();
}