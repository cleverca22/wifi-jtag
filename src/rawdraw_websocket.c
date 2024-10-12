//Copyright 2015-2021 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.  This file mostly based on `cnhttp` from cntools.

#define USE_RAM_MFS


#include <stdint.h>
#include <stdio.h>
#include <string.h>


void NewWebSocket(void);
void  WebSocketGotData( uint8_t c );
void InternalStartHTTP( );
void http_disconnetcb(int conn);

int transitlen;

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} RD_SHA1_CTX;

#define RD_SHA1_DIGEST_SIZE 20

#define HTTP_CONNECTIONS 2
#ifndef MAX_HTTP_PATHLEN
#define MAX_HTTP_PATHLEN 80
#endif

#define HTTP_STATE_WAIT_METHOD 1
#define HTTP_STATE_WAIT_PATH   2
#define HTTP_STATE_WAIT_PROTO  3

#define HTTP_STATE_WAIT_FLAG   4
#define HTTP_STATE_WAIT_INFLAG 5
#define HTTP_STATE_DATA_XFER   7
#define HTTP_STATE_DATA_WEBSOCKET   8

#define HTTP_WAIT_CLOSE        15

struct MFSFileInfo
{
	uint32_t filelen;
#ifdef USE_RAM_MFS
	uint32_t offset;
#else
	FILE * file;
#endif
};

int TCPCanSend( int socket, int size ) {
}

//from http://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
static const char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                      'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                      'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                      'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                      'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                      'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                      'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                      '4', '5', '6', '7', '8', '9', '+', '/'};

static const int mod_table[] = {0, 2, 1};

void my_base64_encode(const unsigned char *data, unsigned int input_length, uint8_t * encoded_data )
{

	int i, j;
    int output_length = 4 * ((input_length + 2) / 3);

    if( !encoded_data ) return;
	if( !data ) { encoded_data[0] = '='; encoded_data[1] = 0; return; }

    for (i = 0, j = 0; i < input_length; ) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[output_length - 1 - i] = '=';

    encoded_data[j] = 0;
}

#define RDrol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
/* FIXME: can we do this in an endian-proof way? */
#ifdef WORDS_BIGENDIAN
#define RDblk0(i) block->l[i]
#else
#define RDblk0(i) (block->l[i] = (RDrol(block->l[i],24)&0xFF00FF00) \
    |(RDrol(block->l[i],8)&0x00FF00FF))
#endif
#define RDblk(i) (block->l[i&15] = RDrol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))
/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+RDblk0(i)+0x5A827999+RDrol(v,5);w=RDrol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+RDblk(i)+0x5A827999+RDrol(v,5);w=RDrol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+RDblk(i)+0x6ED9EBA1+RDrol(v,5);w=RDrol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+RDblk(i)+0x8F1BBCDC+RDrol(v,5);w=RDrol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+RDblk(i)+0xCA62C1D6+RDrol(v,5);w=RDrol(w,30);

/* Hash a single 512-bit block. This is the core of the algorithm. */
static void RD_SHA1_Transform(uint32_t state[5], const uint8_t buffer[64])
{
    uint32_t a, b, c, d, e;
    typedef union {
        uint8_t c[64];
        uint32_t l[16];
    } CHAR64LONG16;
    CHAR64LONG16* block;

#ifdef SHA1HANDSOFF
    static uint8_t workspace[64];
    block = (CHAR64LONG16*)workspace;
    memcpy(block, buffer, 64);
#else
    block = (CHAR64LONG16*)buffer;
#endif

    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
    R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
    R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
    R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
    R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
    R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
    R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
    R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
    R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
    R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
    R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
    R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
    R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
    R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
    R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
    R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
    R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
    R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
    R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
    R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;

    /* Wipe variables */
    a = b = c = d = e = 0;
}

/* SHA1Init - Initialize new context */
static void RD_SHA1_Init(RD_SHA1_CTX* context)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}

/* Run your data through this. */
static void RD_SHA1_Update(RD_SHA1_CTX* context, const uint8_t* data, const unsigned long len)
{
    size_t i, j;

#ifdef VERBOSE
    SHAPrintContext(context, "before");
#endif

    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
    context->count[1] += (len >> 29);
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64-j));
        RD_SHA1_Transform(context->state, context->buffer);
        for ( ; i + 63 < len; i += 64) {
            RD_SHA1_Transform(context->state, data + i);
        }
        j = 0;
    }
    else i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);

#ifdef VERBOSE
    SHAPrintContext(context, "after ");
#endif
}

/* Add padding and return the message digest. */
static void RD_SHA1_Final(uint8_t digest[RD_SHA1_DIGEST_SIZE],RD_SHA1_CTX* context)
{
    uint32_t i;
    uint8_t  finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]
         >> ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */
    }
    RD_SHA1_Update(context, (uint8_t *)"\200", 1);
    while ((context->count[0] & 504) != 448) {
        RD_SHA1_Update(context, (uint8_t *)"\0", 1);
    }
    RD_SHA1_Update(context, finalcount, 8);  /* Should cause a SHA1_Transform() */
    for (i = 0; i < RD_SHA1_DIGEST_SIZE; i++) {
        digest[i] = (uint8_t)
         ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }

    /* Wipe variables */
    i = 0;
    memset(context->buffer, 0, 64);
    memset(context->state, 0, 20);
    memset(context->count, 0, 8);
    memset(finalcount, 0, 8);	/* SWR */

#ifdef SHA1HANDSOFF  /* make SHA1Transform overwrite its own static vars */
    SHA1_Transform(context->state, context->buffer);
#endif
}

struct HTTPConnection
{
	uint8_t  state:4;
	uint8_t  state_deets;

	//Provides path, i.e. "/index.html" but, for websockets, the last 
	//32 bytes of the buffer are used for the websockets key.  
	uint8_t  pathbuffer[MAX_HTTP_PATHLEN];
	uint8_t  is_dynamic:1;
	uint16_t timeout;

	union data_t
	{
		struct MFSFileInfo filedescriptor;
		struct UserData { uint16_t a, b, c; } user;
		struct UserDataPtr { void * v; } userptr;
	} data;

	void * rcb;
	void * rcbDat; //For websockets primarily.
	void * ccb;                          //Close callback (used for websockets, primarily)

	uint32_t bytesleft;
	uint32_t bytessofar;

	uint8_t  is404:1;
	uint8_t  isdone:1;
	uint8_t  isfirst:1;
	uint8_t  keep_alive:1;
	uint8_t  need_resend:1;
	uint8_t  send_pending:1; //If we can send data, we should?
	uint8_t  is_gzip:1;

	int socket;
	uint8_t corked_data[4096];
	int corked_data_place;
};

#define HTDEBUG( x, ... ) printf( x, ##__VA_ARGS__ )

struct HTTPConnection HTTPConnections[HTTP_CONNECTIONS];
struct HTTPConnection * curhttp;
uint8_t * curdata;
uint16_t  curlen;
uint8_t   wsmask[4];
uint8_t   wsmaskplace;

#define HTTPPOP (*curdata++)

int sockets[HTTP_CONNECTIONS];

void et_espconn_disconnect( int socket )
{
	int i;
	//printf( "Shut: %d\n", socket );
	for( i = 0; i < HTTP_CONNECTIONS; i++ )
	{
		if( sockets[i] == socket )
		{
			http_disconnetcb( i );
			sockets[i] = 0;
		}
	}

	if( destroy_sockets[destroy_socket_head] ) closesocket( destroy_sockets[destroy_socket_head] );
	destroy_sockets[destroy_socket_head] = socket;
	destroy_socket_head = (destroy_socket_head+1)%DESTROY_SOCKETS_LIST;

}

void HTTPClose( )
{
	//This is dead code, but it is a testament to Charles.
	//Do not do this here.  Wait for the ESP to tell us the
	//socket is successfully closed.
	//curhttp->state = HTTP_STATE_NONE;
	curhttp->state = HTTP_WAIT_CLOSE;
	et_espconn_disconnect( curhttp->socket ); 
	CloseEvent();
}

void  HTTPGotData( )
{
	uint8_t c;
	curhttp->timeout = 0;
	while( curlen-- )
	{
		c = HTTPPOP;
	//	sendhex2( h->state ); sendchr( ' ' );

		switch( curhttp->state )
		{
		case HTTP_STATE_WAIT_METHOD:
			if( c == ' ' )
			{
				curhttp->state = HTTP_STATE_WAIT_PATH;
				curhttp->state_deets = 0;
			}
			break;
		case HTTP_STATE_WAIT_PATH:
			curhttp->pathbuffer[curhttp->state_deets++] = c;
			if( curhttp->state_deets == MAX_HTTP_PATHLEN )
			{
				curhttp->state_deets--;
			}
			
			if( c == ' ' )
			{
				//Tricky: If we're a websocket, we need the whole header.
				curhttp->pathbuffer[curhttp->state_deets-1] = 0;
				curhttp->state_deets = 0;

				if( strncmp( (const char*)curhttp->pathbuffer, "/d/ws", 5 ) == 0 )
				{
					curhttp->state = HTTP_STATE_DATA_WEBSOCKET;
					curhttp->state_deets = 0;
				}
				else
				{
					curhttp->state = HTTP_STATE_WAIT_PROTO; 
				}
			}
			break;
		case HTTP_STATE_WAIT_PROTO:
			if( c == '\n' )
			{
				curhttp->state = HTTP_STATE_WAIT_FLAG;
			}
			break;
		case HTTP_STATE_WAIT_FLAG:
			if( c == '\n' )
			{
				curhttp->state = HTTP_STATE_DATA_XFER;
				InternalStartHTTP( );
			}
			else if( c != '\r' )
			{
				curhttp->state = HTTP_STATE_WAIT_INFLAG;
			}
			break;
		case HTTP_STATE_WAIT_INFLAG:
			if( c == '\n' )
			{
				curhttp->state = HTTP_STATE_WAIT_FLAG;
				curhttp->state_deets = 0;
			}
			break;
		case HTTP_STATE_DATA_XFER:
			//Ignore any further data?
			curlen = 0;
			break;
		case HTTP_STATE_DATA_WEBSOCKET:
			WebSocketGotData( c );
			break;
		case HTTP_WAIT_CLOSE:
			if( curhttp->keep_alive )
			{
				curhttp->state = HTTP_STATE_WAIT_METHOD;
			}
			else
			{
				HTTPClose( );
			}
			break;
		default:
			break;
		};
	}

}

void http_recvcb(int conn, char *pusrdata, unsigned short length)
{
	int whichhttp = conn;
	//Though it might be possible for this to interrupt the other
	//tick task, I don't know if this is actually a probelem.
	//I'm adding this back-up-the-register just in case.
	if( curhttp ) { HTDEBUG( "Unexpected Race Condition\n" ); return; }

	curhttp = &HTTPConnections[whichhttp];
	curdata = (uint8_t*)pusrdata;
	curlen = length;
	HTTPGotData();
	curhttp = 0 ;

}

int httpserver_connectcb( int socket )
{
	int i;

	for( i = 0; i < HTTP_CONNECTIONS; i++ )
	{
		if( HTTPConnections[i].state == 0 )
		{
			HTTPConnections[i].socket = socket;
			HTTPConnections[i].state = HTTP_STATE_WAIT_METHOD;
			break;
		}
	}
	if( i == HTTP_CONNECTIONS )
	{
		return -1;
	}

	return i;
}

uint8_t WSPOPMASK()
{
	uint8_t mask = wsmask[wsmaskplace];
	wsmaskplace = (wsmaskplace+1)&3;
	return (*curdata++)^(mask);
}

void WebSocketData( int len )
{
	if( curhttp->rcbDat )
	{
		((void(*)( int ))curhttp->rcbDat)(  len ); 
	}
}

#ifndef RD_SHA1_HASH_LEN
#define RD_SHA1_HASH_LEN RD_SHA1_DIGEST_SIZE
#endif

void  WebSocketGotData( uint8_t c )
{
	switch( curhttp->state_deets )
	{
	case 0:
	{
		int i = 0;
		char inkey[120];
		unsigned char hash[RD_SHA1_HASH_LEN];
		RD_SHA1_CTX c;
		int inkeylen = 0;

		curhttp->is_dynamic = 1;
		while( curlen > 20 )
		{
			curdata++; curlen--;
			if( strncmp( curdata, "Sec-WebSocket-Key: ", 19 ) == 0 )
			{
				break;
			}
		}

		if( curlen <= 21 )
		{
			HTDEBUG( "No websocket key found.\n" );
			curhttp->state = HTTP_WAIT_CLOSE;
			return;
		}

		curdata+= 19;
		curlen -= 19;


#define WS_KEY_LEN 36
#define WS_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_RETKEY_SIZEM1 32

		while( curlen > 1 )
		{
			uint8_t lc = *(curdata++);
			inkey[i] = lc;
			curlen--;
			if( lc == '\r' )
			{
				inkey[i] = 0;
				break;
			}
			i++;
			if( i >= sizeof( inkey ) - WS_KEY_LEN - 5 )
			{
				HTDEBUG( "Websocket key too big.\n" );
				curhttp->state = HTTP_WAIT_CLOSE;
				return;
			}
		}
		if( curlen <= 1 )
		{
			HTDEBUG( "Invalid websocket key found.\n" );
			curhttp->state = HTTP_WAIT_CLOSE;
			return;
		}

		if( i + WS_KEY_LEN + 1 >= sizeof( inkey ) )
		{
			HTDEBUG( "WSKEY Too Big.\n" );
			curhttp->state = HTTP_WAIT_CLOSE;
			return;
		}

		memcpy( &inkey[i], WS_KEY, WS_KEY_LEN + 1 );
		i += WS_KEY_LEN;
		RD_SHA1_Init( &c );
		RD_SHA1_Update( &c, inkey, i );
		RD_SHA1_Final( hash, &c );

#if	(WS_RETKEY_SIZE > MAX_HTTP_PATHLEN - 10 )
#error MAX_HTTP_PATHLEN too short.
#endif

		my_base64_encode( hash, RD_SHA1_HASH_LEN, curhttp->pathbuffer + (MAX_HTTP_PATHLEN-WS_RETKEY_SIZEM1)  );

		curhttp->bytessofar = 0;
		curhttp->bytesleft = 0;

		NewWebSocket();
		//Respond...
		curhttp->state_deets = 1;
		break;
	}
	case 1:
		if( c == '\n' ) curhttp->state_deets = 2;
		break;
	case 2:
		if( c == '\r' ) curhttp->state_deets = 3;
		else curhttp->state_deets = 1;
		break;
	case 3:
		if( c == '\n' ) curhttp->state_deets = 4;
		else curhttp->state_deets = 1;
		break;
	case 5: //Established connection.
	{
		//XXX TODO: Seems to malfunction on large-ish packets.  I know it has problems with 140-byte payloads.

		do
		{
			if( curlen < 5 ) //Can't interpret packet.
				break;

			uint8_t fin = c & 1;
			uint8_t opcode = c << 4;
			uint16_t payloadlen = *(curdata++);

			curlen--;
			if( !(payloadlen & 0x80) )
			{
				HTDEBUG( "Unmasked packet (%d)\n", payloadlen );
				curhttp->state = HTTP_WAIT_CLOSE;
				break;
			}

			if( opcode == 128 )
			{
				//Close connection.
				//HTDEBUG( "CLOSE\n" );
				//curhttp->state = HTTP_WAIT_CLOSE;
				//break;
			}

			payloadlen &= 0x7f;
			if( payloadlen == 127 )
			{
				//Very long payload.
				//Not supported.
				HTDEBUG( "Unsupported payload packet.\n" );
				curhttp->state = HTTP_WAIT_CLOSE;
				break;
			}
			else if( payloadlen == 126 )
			{
				payloadlen = (curdata[0] << 8) | curdata[1];
				curdata += 2;
				curlen -= 2;
			}
			wsmask[0] = curdata[0];
			wsmask[1] = curdata[1];
			wsmask[2] = curdata[2];
			wsmask[3] = curdata[3];
			curdata += 4;
			curlen -= 4;
			wsmaskplace = 0;

			//XXX Warning: When packets get larger, they may split the
			//websockets packets into multiple parts.  We could handle this
			//but at the cost of prescious RAM.  I am chosing to just drop those
			//packets on the floor, and restarting the connection.
			if( curlen < payloadlen )
			{
				extern int cork_binary_rx;
				cork_binary_rx = 1;
				//HTDEBUG( "Websocket Fragmented. %d %d\n", curlen, payloadlen );
				//curhttp->state = HTTP_WAIT_CLOSE;
				HTDEBUG( "Websocket Fragmented. %d %d\n", curlen, payloadlen );
				curhttp->state = HTTP_WAIT_CLOSE;
				return;
			}

			char * newcurdata = curdata + payloadlen + 1;
			WebSocketData( payloadlen );
			curlen -= payloadlen; 
			curdata = newcurdata;
		}
		while( curlen > 5 );
		break;
	}
	default:
		break;
	}
}

static void readrdbuffer_websocket_dat(  int len )
{
	do
	{
		int bufferok = TCPCanSend( curhttp->socket, 1340 );
		if( transitlen <= 0 || !bufferok || curhttp->bytesleft == -1 ) return;

		int offset = transitlen * 4 - curhttp->bytesleft;
		int tosend = curhttp->bytesleft;
		if( tosend > 1340 ) tosend = 1340;

		WebSocketSend( NULL, 0 );

		//Special - we send an empty frame to indicate competion.
		if( tosend == 0 )
			curhttp->bytesleft = -1;
		else
			curhttp->bytesleft -= tosend;
	} while(1);
}

static void readrdbuffer_websocket_cmd(  int len )
{
	uint8_t  buf[1300];
	int i;

	if( len > 1300 ) len = 1300;

	for( i = 0; i < len; i++ )
	{
		buf[i] = WSPOPMASK();
	}

	if( strncmp( buf, "SWAP", 4 ) == 0 )
	{
	}
}

void NewWebSocket()
{
	if( strncmp( (const char*)curhttp->pathbuffer, "/d/ws/cmdbuf", 9 ) == 0 )
	{
		printf( "Got connection.\n" );
		curhttp->rcb = (void*)&readrdbuffer_websocket_dat;
		curhttp->rcbDat = (void*)&readrdbuffer_websocket_cmd;
	}
	else
	{
		curhttp->is404 = 1;
	}
}
