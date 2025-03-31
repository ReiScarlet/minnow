#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t up_bound = static_cast<uint64_t>( UINT32_MAX ) + 1;
  const uint32_t checkpoint_seqno = Wrap32::wrap( checkpoint, zero_point ).raw_value_;
  uint32_t dis = this->raw_value_ - checkpoint_seqno;
  if ( dis <= ( up_bound >> 1 ) || checkpoint + dis < up_bound ) {
    return checkpoint + dis;
  }
  return checkpoint + dis - up_bound;
}
