/* 
 * File:   field_iterator.h
 * Author: thek
 *
 * Created on den 8 april 2010, 15:24
 */

#ifndef _FIELD_ITERATOR_H
#define	_FIELD_ITERATOR_H
#include "binlog_event.h"
#include "value.h"
#include "row_of_fields.h"
#include <vector>

using namespace MySQL;

namespace MySQL {

bool is_null(unsigned char *bitmap, int index);

int lookup_metadata_field_size(enum MySQL::system::enum_field_types field_type);
boost::uint32_t extract_metadata(const Table_map_event *map, int col_no);

template <class Iterator_value_type >
class Row_event_iterator : public std::iterator<std::forward_iterator_tag,
                                                Iterator_value_type>
{
public:
  Row_event_iterator() : m_row_event(0), m_table_map(0),
                         m_new_field_offset_calculated(0), m_field_offset(0)
  { }

  Row_event_iterator(const Row_event *row_event,
                     const Table_map_event *table_map)
    : m_row_event(row_event), m_table_map(table_map),
      m_new_field_offset_calculated(0)
  {
      m_field_offset= 0;
  }

  Iterator_value_type operator*();
  
  Row_event_iterator& operator++();
  
  Row_event_iterator operator++(int);

  bool operator==(const Row_event_iterator& x) const;
  
  bool operator!=(const Row_event_iterator& x) const;

  //Row_iterator end() const;
private:
    size_t fields(Iterator_value_type& fields_vector );
    const Row_event *m_row_event;
    const Table_map_event *m_table_map;
    unsigned long m_new_field_offset_calculated;
    unsigned long m_field_offset;
};



template <class Iterator_value_type>
size_t Row_event_iterator<Iterator_value_type>::fields(Iterator_value_type& fields_vector )
{
  size_t field_offset= m_field_offset;
  int row_field_col_index= 0;
  std::string nullbits= m_row_event->row.substr(field_offset,m_row_event->null_bits_len).c_str();
  field_offset += m_row_event->null_bits_len;
  for(unsigned col_no=0; col_no < m_table_map->columns.length(); ++col_no)
  {
    ++row_field_col_index;
    unsigned int type= m_table_map->columns[col_no]&0xFF;
    boost::uint32_t metadata= extract_metadata(m_table_map, col_no);
    MySQL::Value val((enum MySQL::system::enum_field_types)type,
                     metadata,
                     &m_row_event->row[field_offset]);
    if (is_null((unsigned char *)nullbits.c_str(), col_no ))
    {
      val.is_null(true);
    }
    else
    {
       /*
        If the value is null it is not in the list of values and thus we won't
        increse the offset. TODO what if all values are null?!
       */
      field_offset += val.length();
    }
    fields_vector.push_back(val);
  }
  return field_offset;
}

template <class Iterator_value_type >
Iterator_value_type Row_event_iterator<Iterator_value_type>::operator*()
{ // dereferencing
  Iterator_value_type fields_vector;
  /*
   * Remember this offset if we need to increate the row pointer
   */
  m_new_field_offset_calculated= fields(fields_vector);

  return fields_vector;
}

//const Row_of_fields& Row_iterator::operator*() const
//{ // dereferencing
//    return fields();
//}

template< class Iterator_value_type >
Row_event_iterator< Iterator_value_type >&
  Row_event_iterator< Iterator_value_type >::operator++()
{ // preﬁx
  if (m_field_offset < m_row_event->row.length())
  {
    /*
     * If we requested the fields in a previous operations
     * we also calculated the new offset at the same time.
     */
    if (m_new_field_offset_calculated != 0)
    {
      m_field_offset= m_new_field_offset_calculated;
      //m_field_offset += m_row_event->null_bits_len;
      m_new_field_offset_calculated= 0;
      if (m_field_offset >= m_row_event->row.length())
        m_field_offset= 0;
      return *this;
    }

    /*
     * Advance the field offset to the next row
     */
    int row_field_col_index= 0;
    std::string nullbits= m_row_event->row.substr(m_field_offset,m_row_event->null_bits_len).c_str();
    m_field_offset += m_row_event->null_bits_len;
    for(unsigned col_no=0; col_no < m_table_map->columns.length(); ++col_no)
    {
      ++row_field_col_index;
      MySQL::Value val((enum MySQL::system::enum_field_types)m_table_map->columns[col_no],
                       m_table_map->metadata[col_no],
                       &m_row_event->row[m_field_offset]);
      if (!is_null((unsigned char *)nullbits.c_str(), col_no))
      {
        m_field_offset += val.length();
      }
      //fields_vector.add(val);
    }

    return *this;
  }

  m_field_offset= 0;
  return *this;

}

template <class Iterator_value_type >
Row_event_iterator< Iterator_value_type >
  Row_event_iterator< Iterator_value_type >::operator++(int)
{ // postﬁx
  Row_event_iterator temp = *this;
  ++*this;
  return temp;
}

template <class Iterator_value_type >
bool Row_event_iterator< Iterator_value_type >::operator==(const Row_event_iterator& x) const
{
  return m_field_offset == x.m_field_offset;
}

template <class Iterator_value_type >
bool Row_event_iterator< Iterator_value_type >::operator!=(const Row_event_iterator& x) const
{
  return m_field_offset != x.m_field_offset;
}





}


#endif	/* _FIELD_ITERATOR_H */

