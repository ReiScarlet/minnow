#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <string>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return in_flight_cnt_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return retrans_cnt_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t windows_zero = window_size_ == 0 ? 1 : 0;
  uint64_t windows_capacity
    = ( window_size_ + windows_zero ) < in_flight_cnt_ ? 0 : window_size_ + windows_zero - in_flight_cnt_;
  do {
    if ( is_fin_sent ) {
      return;
    }

    uint64_t payload_size = min( reader().bytes_buffered(), TCPConfig::MAX_PAYLOAD_SIZE );
    uint64_t seq_size = min( windows_capacity, payload_size + ( abs_seqno_ == 0 ) );
    payload_size = seq_size;

    TCPSenderMessage msg = TCPSenderMessage();
    if ( abs_seqno_ == 0 ) {
      msg.SYN = true;
      payload_size--;
    }

    if ( reader().has_error() ) {
      msg.RST = true;
    }

    while ( msg.payload.size() < payload_size ) {
      string_view front_view = reader().peek();
      uint64_t bytes_to_read = min( front_view.size(), payload_size - msg.payload.size() );
      msg.payload += front_view.substr( 0, bytes_to_read );
      reader().pop( bytes_to_read );
    }

    if ( reader().is_finished() && seq_size < windows_capacity ) {
      msg.FIN = true;
      seq_size--;
      is_fin_sent = true;
    }

    if ( msg.sequence_length() == 0 ) {
      return;
    }

    msg.seqno = Wrap32::wrap( abs_seqno_, isn_ );
    abs_seqno_ += msg.sequence_length();
    in_flight_cnt_ += msg.sequence_length();
    outstanding_msg_.push_back( msg );
    transmit( msg );
    if ( expire_time_ == UINT16_MAX ) {
      expire_time_ = cur_time_ + cur_RTO_ms_;
    }
    windows_capacity
      = ( window_size_ + windows_zero ) < in_flight_cnt_ ? 0 : window_size_ + windows_zero - in_flight_cnt_;
  } while ( windows_capacity != 0 && reader().bytes_buffered() != 0 );
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return { .seqno = Wrap32::wrap( abs_seqno_, isn_ ),
           .SYN = false,
           .payload = string(),
           .FIN = false,
           .RST = reader().has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.ackno.has_value() ) {
    uint64_t ack_from_recv = msg.ackno->unwrap( isn_, abs_seqno_ );
    if ( ack_from_recv > ackno_ && ack_from_recv <= abs_seqno_ ) {
      ackno_ = ack_from_recv;
      cur_RTO_ms_ = initial_RTO_ms_;
      expire_time_ = cur_time_ + cur_RTO_ms_;
      retrans_cnt_ = 0;
      while ( !outstanding_msg_.empty() ) {
        auto& front_msg = outstanding_msg_.front();
        if ( front_msg.seqno.unwrap( isn_, abs_seqno_ ) + front_msg.sequence_length() > ackno_ ) {
          break;
        }
        in_flight_cnt_ -= front_msg.sequence_length();
        outstanding_msg_.pop_front();
      }
      if ( outstanding_msg_.empty() ) {
        expire_time_ = UINT64_MAX;
      }
    }
  }
  window_size_ = msg.window_size;
  if ( msg.RST ) {
    writer().set_error();
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  cur_time_ += ms_since_last_tick;
  if ( expire_time_ != 0 && cur_time_ >= expire_time_ ) {
    transmit( outstanding_msg_.front() );
    if ( window_size_ != 0 ) {
      retrans_cnt_++;
      cur_RTO_ms_ *= 2;
    }
    expire_time_ = cur_time_ + cur_RTO_ms_;
  }
}
