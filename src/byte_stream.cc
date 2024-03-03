#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity )
  , streams_()
  , push_size_( 0 )
  , pop_size_( 0 )
  , buffer_size_( 0 )
  , is_closed_( false )
  , error_( false )
{}

bool Writer::is_closed() const
{
  return this->is_closed_;
}

void Writer::push( string data )
{
  if ( available_capacity() == 0 || data.empty() ) {
    return;
  }
  const size_t size = data.size();
  const size_t n = min( size, available_capacity() );
  if ( n < size ) {
    data = data.substr( 0, n );
  }
  streams_.push_back( std::move( data ) );
  push_size_ += n;
  buffer_size_ += n;
}

void Writer::close()
{
  this->is_closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_size_;
}

uint64_t Writer::bytes_pushed() const
{
  return push_size_;
}

bool Reader::is_finished() const
{
  if ( is_closed_ && bytes_buffered() == 0 ) {
    return true;
  }
  return false;
}

uint64_t Reader::bytes_popped() const
{
  return pop_size_;
}

string_view Reader::peek() const
{
  return string_view( streams_.front().c_str(), 1 );
}

void Reader::pop( uint64_t len )
{
  size_t n = min( len, buffer_size_ );
  while ( n > 0 ) {
    size_t size = streams_.front().size();
    if ( n < size ) {
      streams_.front().erase( 0, n );
      buffer_size_ -= n;
      pop_size_ += n;
      return;
    }
    streams_.pop_front();
    n -= size;
    buffer_size_ -= size;
    pop_size_ += size;
  }
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_size_;
}
