// Implicit inclusion of signature cache
// All standard C/Posix includes are implied
// All CNG libraries are implied
// Anything non-static in the default/current namespace is included.
//
// Each compiler command line is required to include all files for the current
// namespace. This allows the compiler to pre-parse all function/data
// signatures (which could then be cached). Also, the compiler can detect
// changed files eliminating the need for "make" to detect local changes.
//
// No support for C++ operator overloading, virtual functions, templates,
// exceptions, or headers. Initially not supporting class and new, but
// the class/new decision can be revisited later.


//CNG: int Main( strn args[:], strn env[:] )
//C:
int main( int arg_c, char *argc[], char **envp )
{
	// 1. Performance (typically faster than C)
	// 2. Simplicity (don't over-complicate code generation or syntax)
	// 3. Compatibility (don't break C syntax without due consideration)

	// Event listener
	// Connect handler
	// Data handler
	int socket_fd = socket( AF_INET, SOCKET_STREAM | SOCK_NONBLOCK, 0 );
	listen();


	return( EXIT_SUCCESS );

	// NOTES: Fast transfer interfaces:
	// sendmmsg(), recvmmsg(), sendfile(), and splice()
	// https://lwn.net/Articles/629155/ https://lwn.net/Articles/615238/
}

void
// void specifically implied for non-specified return value(s).
connect_handler( int socket_fd )
{
	int net_fd = accept_connect( socket_fd );

	// Options: A. return to event_listener B. call http_handler directly
	// From an event management standpoint A is the best choice,
	// but from an HTTP optimization standpoint B will be fastest.
	http_handler( net_fd );
	return;
}

http_handler( int net_fd )
{
	// fd has attributes (blocking read/write), but
	// the attributes can be overridden per call.
	// Also, note that blocking can use non-blocking for
	// pseudo-threaded state machine behavior.
	
	// Read until http protocol is satisfied.
	strn http_raw, int http_read_status = http_read( net_fd );
	if( 0 != read_status )
		except read_failed;
		// Hmmm, this is basically "goto" renamed and limited in scope
		//
		// One way or the other what's needed is context sensitive error
		// logging combined with incremental resource allocation unwinding.

	// Howto: Determine when to copy vs. reference.
	//
	// Example: http_words should be a new array of length + pointer,
	// however parse_strn() needs to return a new copy length + data.
	//
	// Solution: Compiler hint via & in caller indicating to reference
	// vs. using a copy of parameter data... however, the called function
	// would use immediate syntax rather than pointer syntax in both cases.
	//
	// Note: Array creators use alloca() of sizeof ulong + initial allocation
	// on the stack (or global heap). Returned copies and shallow copies
	// use length + pointer to original data or managed heap space.

	// Can't beat Lisp for this operation, but this is OK.
	strn http_words[:] = parse_strn( &http_raw );
	cases( upper_strn( http_words[0] ) )
	{
	case `GET`: // back tick string doesn't get null terminator

		// Unary < indicates variable exists one {} nesting level out.
		// This allows creation by first user, disallows users prior
		// to creation, but implies the need for an uninitialized
		// variable detection mechanism.

		<strn url_data, <int url_get_status = url_get( http_words[1] );
		http_write( url_data );
	}
	// cases() case values are compared by value if the types match.
	// Note: Target values may be calculated if the resulting type matches.

	// Perhaps allow listing multiple exceptions
	// Alternative: switch/case style "exception" targets.
	// This would make fall through more natural for unwinding.
	exception read_failed
	{
		logf( 1, "http_read( %S ) failed with %d got [%S]",
			fd_info( net_fd ), read_status, http_raw );
		return;
	}
}
