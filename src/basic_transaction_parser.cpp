#include "binlog_event.h"
#include "basic_transaction_parser.h"
#include "protocol.h"
#include "value.h"
#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "field_iterator.h"

namespace MySQL {

MySQL::Binary_log_event *Basic_transaction_parser::process_event(MySQL::Query_event *qev)
{
  if (qev->query == "BEGIN")
  {
    //std::cout << "Transaction has started!" << std::endl;
    m_transaction_state= STARTING;
  }
  if (qev->query == "COMMIT")
  {
    m_transaction_state= COMMITTING;
  }

  return process_transaction_state(qev);
}

MySQL::Binary_log_event *Basic_transaction_parser::process_event(MySQL::Xid *ev)
{
  m_transaction_state= COMMITTING;
  return process_transaction_state(ev);
}

MySQL::Binary_log_event *Basic_transaction_parser::process_event(MySQL::Table_map_event *ev)
{
  if(m_transaction_state ==IN_PROGRESS)
  {
    m_event_stack.push_back(ev);
    return 0;
  }
  return ev;
}

MySQL::Binary_log_event *Basic_transaction_parser::process_event(MySQL::Row_event *ev)
{
  if(m_transaction_state ==IN_PROGRESS)
  {
    m_event_stack.push_back(ev);
    return 0;
  }
  return ev;
}

MySQL::Binary_log_event *Basic_transaction_parser::process_transaction_state(MySQL::Binary_log_event *incomming_event)
{
  switch(m_transaction_state)
  {
    case STARTING:
    {
      m_transaction_state= IN_PROGRESS;
      m_start_time= incomming_event->header()->timestamp;
      delete incomming_event; // drop the begin event
      return 0;
    }
    case COMMITTING:
    {
      delete incomming_event; // drop the commit event
           
      /**
       * Propagate the start time for the transaction to the newly created
       * event.
       */
      MySQL::Transaction_log_event *trans=  MySQL::create_transaction_log_event();
      trans->header()->timestamp= m_start_time;

      //std::cout << "There are " << m_event_stack.size() << " events in the transaction: ";
      while( m_event_stack.size() > 0)
      {
        MySQL::Binary_log_event *event= m_event_stack.front();
        m_event_stack.pop_front();
        switch(event->get_event_type())
        {
          case MySQL::TABLE_MAP_EVENT:
          {
            /*
             Index the table name with a table id to ease lookup later.
            */
            MySQL::Table_map_event *tm= static_cast<MySQL::Table_map_event *>(event);
            //std::cout << "Indexing table " << tm->table_id << " " << tm->table_name << std::endl;
            //std::cout.flush ();
            trans->m_table_map.insert(MySQL::Event_index_element(tm->table_id,tm));
            trans->m_events.push_back(event);
          }
          break;
          case MySQL::WRITE_ROWS_EVENT:
          case MySQL::DELETE_ROWS_EVENT:
          case MySQL::UPDATE_ROWS_EVENT:
          {
            trans->m_events.push_back(event);
             /*
             * Propagate last known next position
             */
            trans->header()->next_position= event->header()->next_position;
          }
          break;
          default:
            delete event;
         }
      } // end while
      m_transaction_state= NOT_IN_PROGRESS;
      return(trans);
    }
    case NOT_IN_PROGRESS:
    default:
      return incomming_event;
  }

}

Transaction_log_event *create_transaction_log_event(void)
{
    Transaction_log_event *trans= new Transaction_log_event();
    trans->header()->type_code= USER_DEFINED;
    return trans;
};

Transaction_log_event::~Transaction_log_event()
{
  Int_to_Event_map::iterator it;
  for(it = m_table_map.begin(); it != m_table_map.end();)
  {
    /* No need to delete the event here; it happens in the next iteration */
    m_table_map.erase(it++);
  }

  while (m_events.size() > 0)
  {
    Binary_log_event *event= m_events.back();
    m_events.pop_back();
    delete(event);
  }

}

} // end namespace


