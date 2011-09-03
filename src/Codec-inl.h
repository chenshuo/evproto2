
namespace evproto
{

inline void send(struct bufferevent* bev, const RpcMessage& message)
{
  struct evbuffer* buf = evbuffer_new();
  const int byte_size = message.ByteSize();
  const int len = byte_size + 8; // RPC0 + adler32
  const int total_len = len + 4; // length prepend
  evbuffer_expand(buf, len + 4);
  struct evbuffer_iovec vec;
  evbuffer_reserve_space(buf, total_len, &vec, 1);

  uint8_t* start = static_cast<uint8_t*>(vec.iov_base);
  int len_be = htonl(len);
  memcpy(start, &len_be, sizeof len_be);
  start += 4;
  memcpy(start, "RPC0", 4);
  start += 4;
  uint8_t* end = message.SerializeWithCachedSizesToArray(start);
  assert (end - start == byte_size);
  start += byte_size;

  int32_t checkSum = static_cast<int32_t>(
      ::adler32(1,
	static_cast<const Bytef*>(vec.iov_base)+4,
	byte_size + 4));

  checkSum = htonl(checkSum);
  memcpy(start, &checkSum, sizeof checkSum);
  start += 4;

  assert(start - static_cast<uint8_t*>(vec.iov_base) == total_len);
  vec.iov_len = total_len;
  evbuffer_commit_space(buf, &vec, 1);
  bufferevent_write_buffer(bev, buf);
  evbuffer_free(buf);
}

enum ParseErrorCode
{
  kNoError = 0,
  kInvalidLength,
  kCheckSumError,
  kInvalidNameLen,
  kUnknownMessageType,
  kParseError,
};

inline ParseErrorCode parse(const char* buf, int len, RpcMessage* message)
{
  ParseErrorCode error = kNoError;

  // check sum
  int32_t be32 = 0;
  memcpy(&be32, buf + len - 4, sizeof be32);
  int32_t expectedCheckSum = ntohl(be32);
  int32_t checkSum = static_cast<int32_t>(
      ::adler32(1,
                reinterpret_cast<const Bytef*>(buf),
                len - 4));

  if (checkSum == expectedCheckSum)
  {
    if (memcmp(buf, "RPC0", 4) == 0)
    {
      // parse from buffer
      const char* data = buf + 4;
      int32_t dataLen = len - 8;
      if (message->ParseFromArray(data, dataLen))
      {
        error = kNoError;
      }
      else
      {
        error = kParseError;
      }
    }
    else
    {
      error = kUnknownMessageType;
    }
  }
  else
  {
    error = kCheckSumError;
  }

  return error;
}

inline ParseErrorCode read(struct evbuffer* input, RpcChannel* channel)
{
  ParseErrorCode error = kNoError;
  int readable = evbuffer_get_length(input);
  while (readable >= 12)
  {
    int be32 = 0;
    evbuffer_copyout(input, &be32, sizeof be32);
    int len = ntohl(be32);
    if (len > 64*1024*1024 || len < 8)
    {
      error = kInvalidLength;
      break;
    }
    else if (readable >= len + 4)
    {
      RpcMessage message;
      const char* data = reinterpret_cast<char*>(evbuffer_pullup(input, len + 4));
      error = parse(data + 4, len, &message);
      if (error == kNoError)
      {
        channel->onMessage(message);
        evbuffer_drain(input, len + 4);
        readable = evbuffer_get_length(input);
      }
    }
    else
    {
      break;
    }
  }
  return error;
}

}
