#include "byte_stream.hh"
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : buffer_(), capacity_( capacity ), write_cnt_( 0 ), read_cnt_( 0 ), error_( false ), is_closed_( false )
{}

void Writer::push( string data )
{
  if ( is_closed() || data.empty() ) {
    return;
  }
  if ( data.size() > available_capacity() ) {
    data.resize( available_capacity() );
  }
  if ( !data.empty() ) {
    uint64_t write_len = data.size();
    buffer_.push_back( std::move( data ) );
    write_cnt_ += write_len;
  }
}

void Writer::close()
{
  is_closed_ = true;
}

bool Writer::is_closed() const
{
  return is_closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - write_cnt_ + read_cnt_;
}

uint64_t Writer::bytes_pushed() const
{
  return write_cnt_;
}

string_view Reader::peek() const
{
  if ( buffer_.empty() ) {
    return ""sv;
  }
  return buffer_.front();
}
 
void Reader::pop( uint64_t len )
{
  uint64_t size = min( len, bytes_buffered() );
  read_cnt_ += size;
  if ( size >= buffer_.front().size() ) {
    size -= buffer_.front().size();
    buffer_.pop_front();
  }
  if ( size > 0 && size < buffer_.front().size() ) {
    string str = buffer_.front();
    buffer_.pop_front();
    buffer_.push_front( str.substr( size ) );
  }
}

bool Reader::is_finished() const
{
  return is_closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_buffered() const
{
  return write_cnt_ - read_cnt_;
}

uint64_t Reader::bytes_popped() const
{
  return read_cnt_;
}
