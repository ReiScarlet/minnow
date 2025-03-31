#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  if ( checkpoint > 0 && checkpoint <= UINT32_MAX && message.seqno == isn_ ) {
    return;
  }

  if ( message.RST ) {
    reassembler_.reader().set_error();
  }

  if ( !isn_.has_value() ) {
    if ( !message.SYN ) {
      return;
    }
    isn_ = message.seqno;
  }

  uint64_t abs_seq = message.seqno.unwrap( isn_.value(), checkpoint );
  uint64_t stream_index = abs_seq - 1 + ( message.SYN ? 1 : 0 );
  reassembler_.insert( stream_index, std::move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  const uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  const uint64_t capacity = reassembler_.writer().available_capacity();
  const uint16_t windows_size = capacity > UINT16_MAX ? UINT16_MAX : capacity;
  if ( !isn_.has_value() ) {
    return { .ackno = {}, .window_size = windows_size, .RST = reassembler_.writer().has_error() };
  }

  return { .ackno = Wrap32::wrap( checkpoint, isn_.value() ),
           .window_size = windows_size,
           .RST = reassembler_.writer().has_error() };
}
