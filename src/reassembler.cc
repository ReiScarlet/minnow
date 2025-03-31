#include "reassembler.hh"
#include "byte_stream.hh"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <utility>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& writer = output_.writer();
  uint64_t unacceptable_index = expect_index_ + writer.available_capacity();
  if ( first_index >= unacceptable_index || writer.is_closed() || writer.available_capacity() == 0 ) {
    return;
  }

  if ( first_index + data.length() > unacceptable_index ) {
    data.resize( unacceptable_index - first_index );
    is_last_substring = false;
  }

  if ( is_last_substring ) {
    has_last_substring_ = true;
  }

  if ( first_index > expect_index_ ) {
    cache_bytes( first_index, data );
  } else {
    push_bytes( first_index, data, is_last_substring );
  }
  flush_buffer();
}

void Reassembler::push_bytes( uint64_t first_index, std::string data, bool is_last_substring )
{
  if ( first_index < expect_index_ ) {
    data.erase( 0, expect_index_ - first_index );
  }

  expect_index_ += data.size();
  output_.writer().push( std::move( data ) );

  if ( is_last_substring ) {
    output_.writer().close();
    buffer_.clear();
    bytes_stored_ = 0;
    has_last_substring_ = false;
  }
}

void Reassembler::cache_bytes( uint64_t first_index, std::string data )
{
  // 找出第一个 左节点右边界 大于等于 first_index
  auto left = std::lower_bound(
    buffer_.begin(), buffer_.end(), first_index, []( std::pair<uint64_t, std::string>& item, uint64_t index ) {
      return index > ( item.first + item.second.length() );
    } );

  // 找出第一个 右节点的左边界 大于 end_index
  auto right = std::upper_bound(
    buffer_.begin(),
    buffer_.end(),
    first_index + data.length(),
    []( uint64_t index, std::pair<uint64_t, std::string>& item ) { return index < item.first; } );

  if ( const uint64_t end_index = first_index + data.length(); left != buffer_.end() ) {
    auto& [left_index, left_data] = *left;
    const uint64_t right_index = left_index + left_data.length();

    // right_index >= first_index
    if ( left_index <= first_index && end_index <= right_index ) { // 被包含
      return;
    } else if ( left_index > end_index ) { // 这怎么可能发生
      right = left;
    } else if ( !( left_index >= first_index && right_index <= end_index ) ) {
      // 不是被包含,也不是包含,也不是没有关系,那就是重叠
      if ( first_index >= left_index ) {
        data.insert( 0, std::string_view( left_data.c_str(), left_data.length() - ( right_index - first_index ) ) );
      } else {
        data.resize( data.length() - ( end_index - left_index ) );
        data.append( left_data );
      }
      first_index = min( first_index, left_index );
    }
  }

  if ( const uint64_t end_index = first_index + data.length(); right != left && !buffer_.empty() ) {
    // right的prev才可能和当前数据有关系
    auto& [left_index, left_data] = *prev( right );
    if ( const uint64_t right_index = left_index + left_data.length(); right_index > end_index ) {
      data.resize( data.length() - ( end_index - left_index ) );
      data.append( left_data );
    }
  }

  for ( ; left != right; left = buffer_.erase( left ) ) {
    bytes_stored_ -= left->second.length();
  }
  bytes_stored_ += data.length();

  buffer_.insert( left, { first_index, std::move( data ) } );
}

void Reassembler::flush_buffer()
{
  while ( !buffer_.empty() ) {
    auto& [index, data] = buffer_.front();
    if ( index > expect_index_ ) {
      break;
    }
    bytes_stored_ -= data.length();

    // 如果这是最后一个缓存数据且已经标记接收到最后子串，则传递结束标志
    bool is_last = has_last_substring_ && buffer_.size() == 1;

    push_bytes( index, std::move( data ), is_last );

    if ( !buffer_.empty() ) {
      buffer_.pop_front();
    }
  }

  // 处理边缘情况：如果缓冲区为空但收到了最后子串标记，需要关闭流
  if ( buffer_.empty() && has_last_substring_ ) {
    output_.writer().close();
    has_last_substring_ = false;
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return bytes_stored_;
}
