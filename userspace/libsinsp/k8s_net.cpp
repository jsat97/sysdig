//
// k8s_net.cpp
//

#include "k8s_net.h"
#include "k8s_component.h"
#include "k8s.h"
#include "sinsp.h"
#include "sinsp_int.h"
#include <sstream>
#include <utility>
#include <memory>


k8s_net::k8s_net(k8s& kube, const std::string& uri, const std::string& api) : m_k8s(kube),
		m_uri(uri + api),
		m_stopped(true),
		m_collector(kube.watch_in_thread())
#ifndef K8S_DISABLE_THREAD
		,m_thread(0)
#endif
{
	try
	{
		init();
	}
	catch(...)
	{
		cleanup();
		throw;
	}
}

k8s_net::~k8s_net()
{
	end_thread();
	cleanup();
}

void k8s_net::cleanup()
{
	for (auto& component : k8s_component::list)
	{
		delete m_api_interfaces[component.first];
	}
	m_api_interfaces.clear();
}

void k8s_net::init()
{
	std::string uri = m_uri.to_string();
	std::string::size_type endpos = uri.find_first_of('@');
	if(endpos != std::string::npos)
	{
		std::string::size_type beginpos = uri.find("://") + 3;
		if(beginpos != std::string::npos)
		{
			m_creds = uri.substr(beginpos, endpos - beginpos);
		}
		else
		{
			throw sinsp_exception("Bad URI");
		}
	}

	for (auto& component : k8s_component::list)
	{
		m_api_interfaces[component.first] = 0;
	}
}

void k8s_net::watch()
{
	bool in_thread = m_k8s.watch_in_thread();
#ifdef K8S_DISABLE_THREAD
	if(in_thread)
	{
		g_logger.log("Thread run requested for non-thread binary.", sinsp_logger::SEV_WARNING);
	}
#else
	if(m_stopped && in_thread)
	{
		subscribe();
		m_stopped = false;
		m_thread = new std::thread(&k8s_collector::get_data, &m_collector);
	}
	else
#endif // K8S_DISABLE_THREAD
	if(!in_thread)
	{
		if(!m_collector.subscription_count())
		{
			subscribe();
		}
		m_collector.get_data();
	}
}
	
void k8s_net::subscribe()
{
	for (auto& api : m_api_interfaces)
	{
		m_collector.add(api.second);
	}
}

void k8s_net::unsubscribe()
{
	m_collector.stop();
	m_collector.remove_all();
}

void k8s_net::end_thread()
{
#ifndef K8S_DISABLE_THREAD
	if(m_thread)
	{
		m_thread->join();
		delete m_thread;
		m_thread = 0;
	}
#endif
}

void k8s_net::stop_watching()
{
	if(!m_stopped)
	{
		m_stopped = true;
		unsubscribe();
		end_thread();
	}
}

void k8s_net::get_all_data(const k8s_component::component_map::value_type& component, std::ostream& out)
{
	if(m_api_interfaces[component.first] == 0)
	{
		std::string protocol = m_uri.get_scheme();
		std::ostringstream os;
		os << m_uri.get_host();
		int port = m_uri.get_port();
		if(port)
		{
			os << ':' << port;
		}
		m_api_interfaces[component.first] = new k8s_http(m_k8s, component.second, os.str(), protocol, m_creds);
	}
	
	if(!m_api_interfaces[component.first]->get_all_data(out))
	{
		std::string err;
		std::ostringstream* ostr = dynamic_cast<std::ostringstream*>(&out);
		if(ostr) { err = ostr->str(); }
		throw sinsp_exception(std::string("An error occured while trying to retrieve data for k8s ")
			.append(component.second).append(": ").append(err));
	}
}

