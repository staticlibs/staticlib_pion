// WebSocket, v1.00 2012-09-13
//
// Description: WebSocket RFC6544 codec, written in C++.
// Homepage: http://katzarsky.github.com/WebSocket
// Author: katzarsky@gmail.com

#include "WebSocket.h"

//#include "md5/md5.h"
#include "base64/base64.h"
#include "sha1/sha1.h"

#include <iostream>
#include <string>
#include <string.h>
#include <vector>

using namespace std;

WebSocket::WebSocket() {

}

// TODO: Переделать парсинг чтобы он точно возвраща это WS сообщение или нет, а не так как сейчас.
WebSocketFrameType WebSocket::parseHandshake(const std::string &input_frame) //unsigned char* input_frame, int input_len)
{
	// 1. copy char*/len into string
	// 2. try to parse headers until \r\n occurs
//	string headers((char*)input_frame, input_len);
    string headers(input_frame);
    size_t header_end = headers.find("\r\n\r\n");

	if(header_end == string::npos) { // end-of-headers not found - do not parse
        return WebSocketFrameType::INCOMPLETE_FRAME;
	}

	headers.resize(header_end); // trim off any data we don't need after the headers
	vector<string> headers_rows = explode(headers, string("\r\n"));
    for(size_t i=0; i<headers_rows.size(); i++) {
		string& header = headers_rows[i];
		if(header.find("GET") == 0) {
			vector<string> get_tokens = explode(header, string(" "));
			if(get_tokens.size() >= 2) {
				this->resource = get_tokens[1];
			}
		}
		else {
            size_t pos = header.find(":");
			if(pos != string::npos) {
				string header_key(header, 0, pos);
				string header_value(header, pos+1);
				header_value = trim(header_value);
				if(header_key == "Host") this->host = header_value;
				else if(header_key == "Origin") this->origin = header_value;
				else if(header_key == "Sec-WebSocket-Key") this->key = header_value;
				else if(header_key == "Sec-WebSocket-Protocol") this->protocol = header_value;
			}
		}
	}

	//this->key = "dGhlIHNhbXBsZSBub25jZQ==";
	//printf("PARSED_KEY:%s \n", this->key.data());

    return WebSocketFrameType::OPENING_FRAME;
}

string WebSocket::trim(string str) 
{
	//printf("TRIM\n");
    auto whitespace = std::string {" \t\r\n"};
	string::size_type pos = str.find_last_not_of(whitespace);
	if(pos != string::npos) {
		str.erase(pos + 1);
		pos = str.find_first_not_of(whitespace);
		if(pos != string::npos) str.erase(0, pos);
	}
	else {
		return string();
	}
	return str;
}

vector<string> WebSocket::explode(	
	string  theString,
    string  theDelimiter,
    bool    theIncludeEmptyStrings)
{
	//printf("EXPLODE\n");
	//UASSERT( theDelimiter.size(), >, 0 );
	
	vector<string> theStringVector;
    size_t  start = 0, end = 0, length = 0;

	while ( end != string::npos )
	{
		end = theString.find( theDelimiter, start );

		// If at end, use length=maxLength.  Else use length=end-start.
		length = (end == string::npos) ? string::npos : end - start;

		if (theIncludeEmptyStrings
			|| (   ( length > 0 ) /* At end, end == length == string::npos */
            && ( start  < theString.size() ) ) )
		theStringVector.push_back( theString.substr( start, length ) );

		// If at end, use start=maxSize.  Else use start=end+delimiter.
		start = (   ( end > (string::npos - theDelimiter.size()) )
              ?  string::npos  :  end + theDelimiter.size()     );
	}
	return theStringVector;
}

string WebSocket::answerHandshake() 
{
    unsigned char digest[20]; // 160 bit sha1 digest

	string answer;
	answer += "HTTP/1.1 101 Switching Protocols\r\n";
	answer += "Upgrade: WebSocket\r\n";
	answer += "Connection: Upgrade\r\n";
	if(this->key.length() > 0) {
		string accept_key;
		accept_key += this->key;
		accept_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; //RFC6544_MAGIC_KEY

		//printf("INTERMEDIATE_KEY:(%s)\n", accept_key.data());

		SHA1 sha;
		sha.Input(accept_key.data(), accept_key.size());
		sha.Result((unsigned*)digest);
		
		//printf("DIGEST:"); for(int i=0; i<20; i++) printf("%02x ",digest[i]); printf("\n");

		//little endian to big endian
		for(int i=0; i<20; i+=4) {
			unsigned char c;

			c = digest[i];
			digest[i] = digest[i+3];
			digest[i+3] = c;

			c = digest[i+1];
			digest[i+1] = digest[i+2];
			digest[i+2] = c;
		}

		//printf("DIGEST:"); for(int i=0; i<20; i++) printf("%02x ",digest[i]); printf("\n");

		accept_key = base64_encode((const unsigned char *)digest, 20); //160bit = 20 bytes/chars

		answer += "Sec-WebSocket-Accept: "+(accept_key)+"\r\n";
	}
	if(this->protocol.length() > 0) {
		answer += "Sec-WebSocket-Protocol: "+(this->protocol)+"\r\n";
	}
	answer += "\r\n";
	return answer;

	//return WS_OPENING_FRAME;
}

//int WebSocket::makeFrame(WebSocketFrameType frame_type, unsigned char* msg, int msg_length, unsigned char* buffer, int buffer_size)
std::string WebSocket::makeFrame(WebSocketFrameType frame_type, const std::string& msg)
{
//	  int pos = 0;
    std:: string buffer{};
    int size = msg.size();
    buffer.push_back((unsigned char)frame_type);// = (unsigned char)frame_type; // text frame

	if(size <= 125) {
		buffer.push_back(size);// = size;
	}
	else if(size <= 65535) {
		buffer.push_back(126);// = 126; //16 bit length follows
		
		buffer.push_back((size >> 8) & 0xFF);// = (size >> 8) & 0xFF; // leftmost first
		buffer.push_back(size & 0xFF);// = size & 0xFF;
	}
	else { // >2^16-1 (65535)
		buffer.push_back(127);// = 127; //64 bit length follows
		
		// write 8 bytes length (significant first)
		
		// since msg_length is int it can be no longer than 4 bytes = 2^32-1
		// padd zeroes for the first 4 bytes
		for(int i=3; i>=0; i--) {
			buffer.push_back(0);// = 0;
		}
		// write the actual 32bit msg_length in the next 4 bytes
		for(int i=3; i>=0; i--) {
			buffer.push_back(((size >> 8*i) & 0xFF));// = ((size >> 8*i) & 0xFF);
		}
	}

    buffer.append(msg);
    return buffer;
}


// TODO:: parse Closing Frame
//WebSocketFrameType WebSocket::getFrame(unsigned char* in_buffer, int in_length, unsigned char* out_buffer, int out_size, int* out_length)
WebSocketFrameType WebSocket::getFrame(const std::string& input_frame, std::string& out_message)
{
    size_t in_length = input_frame.size();
    if(in_length < 2) return WebSocketFrameType::INCOMPLETE_FRAME;

    unsigned char msg_opcode = input_frame[0] & 0x0F;
    unsigned char msg_fin = (input_frame[0] >> 7) & 0x01;
    unsigned char msg_masked = (input_frame[1] >> 7) & 0x01;

    if((msg_opcode == 0x8) && (in_length < 3)) return WebSocketFrameType::CLOSING_FRAME;

    // *** message decoding
    size_t payload_length = 0;
    size_t pos = 2;
    size_t length_field = input_frame[1] & 0x7F;
    unsigned int mask = 0;

    if(length_field <= 125) {
        payload_length = length_field;
    }
    else if(length_field == 126) { //msglen is 16bit!
        payload_length = (
            (input_frame[2] << 8) |
            (input_frame[3])
        );
        pos += 2;
    }
//    else if(length_field == 127) { //msglen is 64bit!
//        payload_length = (
//            (in_buffer[2] << 56) |
//            (in_buffer[3] << 48) |
//            (in_buffer[4] << 40) |
//            (in_buffer[5] << 32) |
//            (in_buffer[6] << 24) |
//            (in_buffer[7] << 16) |
//            (in_buffer[8] << 8) |
//            (in_buffer[9])
//        );
//        pos += 8;
//    }
		
    if(in_length < payload_length+pos) {
        return WebSocketFrameType::INCOMPLETE_FRAME;
    }
    std::vector<char> unmasked_message;
    if(msg_masked) {
        mask = *((unsigned int*)(input_frame.c_str()+pos));
        pos += 4;

        // unmask data:
        const char* c = input_frame.c_str() + pos;
        for(size_t i=0; i<payload_length; i++) {
            unmasked_message.push_back(c[i] ^ ((unsigned char*)(&mask))[i%4]);
        }
    } else {
        for(size_t i=pos; i<input_frame.length(); i++) {
            unmasked_message.push_back(input_frame[i]);
        }
    }
	
//    if(payload_length > out_size) {
//        //TODO: if output buffer is too small -- ERROR or resize(free and allocate bigger one) the buffer ?
//    }
    out_message = std::string(unmasked_message.data(), (int )unmasked_message.size());


//    std::cout << "************* out_message [" << out_message << "]" << std::endl;

    if(msg_opcode == 0x0) return (msg_fin)? WebSocketFrameType::TEXT_FRAME : WebSocketFrameType::INCOMPLETE_TEXT_FRAME; // continuation frame ?
    if(msg_opcode == 0x1) return (msg_fin)? WebSocketFrameType::TEXT_FRAME : WebSocketFrameType::INCOMPLETE_TEXT_FRAME;
    if(msg_opcode == 0x2) return (msg_fin)? WebSocketFrameType::BINARY_FRAME : WebSocketFrameType::INCOMPLETE_BINARY_FRAME;
    if(msg_opcode == 0x8) return WebSocketFrameType::CLOSING_FRAME;
    if(msg_opcode == 0x9) return WebSocketFrameType::PING_FRAME;
    if(msg_opcode == 0xA) return WebSocketFrameType::PONG_FRAME;

    return WebSocketFrameType::ERROR_FRAME;
}
