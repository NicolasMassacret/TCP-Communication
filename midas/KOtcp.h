//
// Name: KOtcp.h
// Description: K.O.'s very own socket library
// Author: K.Olchanski
// Date: 11 Aug 1998
// Date: July 2017, exceptions removed
//

#ifndef KOtcpH
#define KOtcpH

#include <string>
#include <vector>

typedef unsigned int KOtcpType;

class KOtcpError
{
 public:
    bool error = false;
    int xerrno = 0;
    std::string message;
 public:
    KOtcpError(); // ctor
    KOtcpError(const char* func, const char* text);
    KOtcpError(const char* func, int xerrno, const char* text);
};

class KOtcpConnection
{
 public: // status flags
    bool fConnected = false;
    std::string fHostname;
    std::string fService;

 public: // settings
    int fConnectTimeoutMilliSec = 5000;
    int fReadTimeoutMilliSec = 5000;
    int fWriteTimeoutMilliSec = 5000;
    bool fHttpKeepOpen = true;

 public: // state
    int fSocket = -1;

 public: // public api
    KOtcpConnection(const char* hostname, const char* service); // ctor
    ~KOtcpConnection(); // dtor

    KOtcpError Connect();
    KOtcpError Close();

    KOtcpError BytesAvailable(int *nbytes);
    KOtcpError WaitBytesAvailable(int wait_time_millisec, int *nbytes);

    KOtcpError WriteString(const std::string& s);
    KOtcpError WriteBytes(const char* ptr, int len);

    KOtcpError ReadString(std::string* s, unsigned max_length);
    KOtcpError ReadHttpHeader(std::string* s);
    KOtcpError ReadBytes(char* ptr, int len);

    KOtcpError HttpGet(const std::vector<std::string>& headers, const char* url, std::vector<std::string> *reply_headers, std::string *reply_body);
    KOtcpError HttpPost(const std::vector<std::string>& headers, const char* url, const std::string& body, std::vector<std::string> *reply_headers, std::string *reply_body);
    KOtcpError HttpPost(const std::vector<std::string>& headers, const char* url, const char* body, int body_length, std::vector<std::string> *reply_headers, std::string *reply_body);
    KOtcpError HttpReadResponse(std::vector<std::string> *reply_headers, std::string *reply_body);

 public: // internal stuff
    int fBufSize = 0; // size of buffer
    int fBufUsed = 0; // bytes stored in buffer
    int fBufPtr  = 0; // first unread byte in buffer
    char* fBuf = NULL;
    bool CopyBuf(std::string *s);
    bool CopyBufHttp(std::string *s);
    KOtcpError ReadBuf();
};

#endif
// end file
