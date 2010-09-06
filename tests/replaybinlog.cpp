/* 
 * File:   main.cpp
 * Author: thek
 *
 * Created on den 12 maj 2010, 14:47
 */

#include <stdlib.h>
#include <boost/foreach.hpp>
#include "binlog_api.h"

bool is_text_field(Value &val)
{
  if (val.type() == MySQL::system::MYSQL_TYPE_VARCHAR ||
      val.type() == MySQL::system::MYSQL_TYPE_BLOB ||
      val.type() == MySQL::system::MYSQL_TYPE_MEDIUM_BLOB ||
      val.type() == MySQL::system::MYSQL_TYPE_LONG_BLOB)
    return true;
  return false;
}

void table_insert(std::string table_name, MySQL::Row_of_fields &fields)
{
  std::cout << "INSERT INTO "
           << table_name
           << " VALUES (";
  MySQL::Row_of_fields::iterator field_it= fields.begin();
  MySQL::Converter converter;
  do {
    /*
     Each row contains a vector of Value objects. The converter
     allows us to transform the value into another
     representation.
    */
    std::string str;
    converter.to(str, *field_it);
    if (is_text_field(*field_it))
      std::cout << '\'';
    std::cout << str;
    if (is_text_field(*field_it))
      std::cout << '\'';
    ++field_it;
    if (field_it != fields.end())
      std::cout << ", ";
  } while(field_it != fields.end());
  std::cout << ")" << std::endl;
}


void table_update(std::string table_name, MySQL::Row_of_fields &old_fields, MySQL::Row_of_fields &new_fields)
{
  std::cout << "UPDATE "
           << table_name
           << " SET ";

  int field_id= 0;
  MySQL::Row_of_fields::iterator field_it= new_fields.begin();
  MySQL::Converter converter;
  do {
    std::string str;
    converter.to(str, *field_it);
    std::cout << field_id << "= ";
    if (is_text_field(*field_it))
      std::cout << '\'';
    std::cout << str;
    if (is_text_field(*field_it))
      std::cout << '\'';
    ++field_it;
    ++field_id;
    if (field_it != new_fields.end())
      std::cout << ", ";
  } while(field_it != new_fields.end());
  std::cout << " WHERE ";
  field_it= old_fields.begin();
  field_id= 0;
  do {
    std::string str;
    converter.to(str, *field_it);
    std::cout << field_id << "= ";
    if (is_text_field(*field_it))
      std::cout << '\'';
    std::cout << str;
    if (is_text_field(*field_it))
      std::cout << '\'';
    ++field_it;
    ++field_id;
    if (field_it != old_fields.end())
      std::cout << " AND ";
  } while(field_it != old_fields.end());
  std::cout << " LIMIT 1" << std::endl;

}


void table_delete(std::string table_name, MySQL::Row_of_fields &fields)
{
  std::cout << "DELETE FROM "
           << table_name
           << " WHERE ";
  MySQL::Row_of_fields::iterator field_it= fields.begin();
  int field_id= 0;
  MySQL::Converter converter;
  do {
    
    std::string str;
    converter.to(str, *field_it);
    std::cout << field_id << "= ";
    if (is_text_field(*field_it))
      std::cout << '\'';
    std::cout << str;
    if (is_text_field(*field_it))
      std::cout << '\'';
    ++field_it;
    ++field_id;
    if (field_it != fields.end())
      std::cout << " AND ";
  } while(field_it != fields.end());
  std::cout << " LIMIT 1" << std::endl;
}

class Incident_handler : public MySQL::Content_handler
{
public:
 Incident_handler() : MySQL::Content_handler() {}

 MySQL::Binary_log_event *process_event(MySQL::Incident_event *incident)
 {
   std::cout << "Event type: "
             << MySQL::system::get_event_type_str(incident->get_event_type())
             << " length: " << incident->header()->event_length
             << " next pos: " << incident->header()->next_position
             << std::endl;
   std::cout << "type= "
             << (unsigned)incident->type
             << " message= "
             << incident->message
             <<  std::endl
             <<  std::endl;
   /* Consume the event */
   delete incident;
   return 0;
 }
};

class Replay_binlog : public MySQL::Content_handler
{
public:
  Replay_binlog() : MySQL::Content_handler() {}
  MySQL::Binary_log_event *process_event(MySQL::Binary_log_event *event)
  {
    if (event->get_event_type() != MySQL::USER_DEFINED)
      return event;
    std::cout << "Event type: "
            << MySQL::system::get_event_type_str(event->get_event_type())
            << " length: " << event->header()->event_length
            << " next pos: " << event->header()->next_position
            << std::endl;
    MySQL::Transaction_log_event *trans= static_cast<MySQL::Transaction_log_event *>(event);
    int row_count=0;
    /*
      The transaction event we created has aggregated all row events in an
      ordered list.
    */
    BOOST_FOREACH(MySQL::Binary_log_event * event, trans->m_events)
    {
      switch (event->get_event_type())
      {
        case MySQL::WRITE_ROWS_EVENT:
        case MySQL::DELETE_ROWS_EVENT:
        case MySQL::UPDATE_ROWS_EVENT:
        MySQL::Row_event *rev= static_cast<MySQL::Row_event *>(event);
        boost::uint64_t table_id= rev->table_id;
        // BUG: will create a new event header if the table id doesn't exist.
        Binary_log_event * tmevent= trans->table_map()[table_id];
        MySQL::Table_map_event *tm;
        if (tmevent != NULL)
          tm= static_cast<MySQL::Table_map_event *>(tmevent);
        else
        {
          std::cout << "Table id "
                    << table_id
                    << " was not registered by any preceding table map event."
                    << std::endl;
          continue;
        }
        /*
         Each row event contains multiple rows and fields. The Row_iterator
         allows us to iterate one row at a time.
        */
        MySQL::Row_event_set rows(rev, tm);
        /*
         Create a fuly qualified table name
        */
        std::ostringstream os;
        os << tm->db_name << '.' << tm->table_name;
        MySQL::Row_event_set::iterator it= rows.begin();
        do {
          MySQL::Row_of_fields fields= *it;

          if (event->get_event_type() == MySQL::WRITE_ROWS_EVENT)
                 table_insert(os.str(),fields);
          if (event->get_event_type() == MySQL::UPDATE_ROWS_EVENT)
          {
            ++it;
            MySQL::Row_of_fields fields2= *it;
            table_update(os.str(),fields,fields2);
          }
          if (event->get_event_type() == MySQL::DELETE_ROWS_EVENT)
            table_delete(os.str(),fields);
        } while (++it != rows.end());
      } // end switch
    } // end BOOST_FOREACH
    /* Consume the event */
    delete trans;
    return 0;
  }
};
/*
 * 
 */
int main(int argc, char** argv)
{
  if (argc != 2)
  {
    fprintf(stderr,"Usage:\n\treplaybinlog URL\n\nExample:\n\treplaybinlog mysql://root:mypasswd@127.0.0.1:3306\n\n");
    return (EXIT_FAILURE);
  }

  MySQL::Binary_log binlog(MySQL::create_transport(argv[1]));


  /*
    Attach a custom event parser which produces user defined events
  */
  MySQL::Basic_transaction_parser transaction_parser;
  Incident_handler incident_hndlr;
  Replay_binlog replay_hndlr;

  binlog.content_handler_pipeline()->push_back(&transaction_parser);
  binlog.content_handler_pipeline()->push_back(&incident_hndlr);
  binlog.content_handler_pipeline()->push_back(&replay_hndlr);

  if (binlog.connect())
  {
    fprintf(stderr,"Can't connect to the master.\n");
    return (EXIT_FAILURE);
  }

  binlog.position("searchbin.000001",4);

  Binary_log_event  *event;

  bool quit= false;
  while(!quit)
  {
    /*
     Pull events from the master. This is the heart beat of the event listener.
    */
    binlog.wait_for_next_event(event);

    /*
     Print the event
    */
    std::cout << "Event type: "
              << MySQL::system::get_event_type_str(event->get_event_type())
              << " length: " << event->header()->event_length
              << " next pos: " << event->header()->next_position
              << std::endl;

    /*
     Perform a special action based on event type
    */

    switch(event->header()->type_code)
    {
    case MySQL::QUERY_EVENT:
      {
        const MySQL::Query_event *qev= static_cast<const MySQL::Query_event *>(event);
        std::cout << "query= "
                  << qev->query
                  << " db= "
                  << qev->db_name
                  <<  std::endl
                  <<  std::endl;
        if (qev->query.find("DROP TABLE REPLICATION_LISTENER") != std::string::npos)
        {
          quit= true;
        }
      }
      break;
    
    case MySQL::ROTATE_EVENT:
      {
        MySQL::Rotate_event *rot= static_cast<MySQL::Rotate_event *>(event);
        std::cout << "filename= "
                  << rot->binlog_file.c_str()
                  << " pos= "
                  << rot->binlog_pos
                  << std::endl
                  << std::endl;
      }
      break;
    
    } // end switch
    delete event;
  } // end loop
  return (EXIT_SUCCESS);
}

