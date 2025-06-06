#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include "glibdictclient.h"

#define DEFAULT_RECEIVE_TEXT_LEN 6144

#define pstrnullv( pstrv ) do { if( ( pstrv ) != NULL ) *( pstrv ) = NULL; } while( FALSE )
#define pstrallocv_number( pstrv, number ) do { if( ( pstrv ) != NULL ) *( pstrv ) = g_new( gchar*, (number) ); } while( FALSE )
#define pstrnullv_index( pstrv, index ) do { if( ( pstrv ) != NULL ) (*( pstrv ))[(index)] = NULL; } while( FALSE )
#define pstrfreev( pstrv ) do { if( ( pstrv ) != NULL ) g_strfreev( *( pstrv ) ); } while( FALSE )

struct _DictResponse
{
	glong *code;
	glong *number;
	gchar **message;
	gchar **word;
	gchar **database;
	gchar **description;
};
typedef struct _DictResponse DictResponse;

struct _DictClient
{
	GObject parent_instance;

	gchar *host;
	guint16 port;

	GSocketClient *socket;
	GIOStream *iostream;
	GDataInputStream *data_input;
	GDataOutputStream *data_output;
};
typedef struct _DictClient DictClient;

enum _DictClientPropertyID
{
	PROP_0, /* 0 is reserved for GObject */

	PROP_HOST,
	PROP_PORT,

	N_PROPS
};
typedef enum _DictClientPropertyID DictClientPropertyID;

static GParamSpec *object_props[N_PROPS] = { NULL, };

G_DEFINE_QUARK( g-dict-client-error-quark, dict_client_error )

G_DEFINE_FINAL_TYPE( DictClient, dict_client, G_TYPE_OBJECT )

static void
dict_client_init(
	DictClient *self )
{
	const GValue *value;

	value = g_param_spec_get_default_value( object_props[PROP_HOST] );
	self->host = g_value_dup_string( value );

	value = g_param_spec_get_default_value( object_props[PROP_PORT] );
	self->port = g_value_get_uint( value );
}

static void
dict_client_dispose(
	GObject *object )
{
	DictClient *self = DICT_CLIENT( object );

	g_clear_object( &self->data_input );
	g_clear_object( &self->data_output );
	g_clear_object( &self->iostream );
	g_clear_object( &self->socket );

	G_OBJECT_CLASS( dict_client_parent_class )->dispose( object );
}

static void
dict_client_finalize(
	GObject *object )
{
	DictClient *self = DICT_CLIENT( object );

	g_clear_pointer( &self->host, g_free );

	G_OBJECT_CLASS( dict_client_parent_class )->finalize( object );
}

static void
dict_client_get_property(
	GObject *object,
	guint prop_id,
	GValue *value,
	GParamSpec *pspec )
{
	DictClient *self = DICT_CLIENT( object );

	switch( (DictClientPropertyID)prop_id )
	{
		case PROP_HOST:
			g_value_set_string( value, self->host );
			break;
		case PROP_PORT:
			g_value_set_uint( value, self->port );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
dict_client_class_init(
	DictClientClass *klass )
{
	GObjectClass *object_class = G_OBJECT_CLASS( klass );

	object_class->get_property = dict_client_get_property;
	object_class->dispose = dict_client_dispose;
	object_class->finalize = dict_client_finalize;

	object_props[PROP_HOST] = g_param_spec_string(
		"host",
		"Host address and port",
		"Address of the connected host and optional port number",
		NULL,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	object_props[PROP_PORT] = g_param_spec_uint(
		"port",
		"Port number",
		"Number of the connected host's port",
		0,
		G_MAXUINT16,
		2628,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS );
	g_object_class_install_properties( object_class, N_PROPS, object_props );
}

/**
\anchor unbracket_string
\brief Finds a bracketed substring.

Scans \c line for pair of double or single brackets (<tt>\"\"</tt> or <tt>''</tt>). Ignores leading and trailing whitespace characters ( <tt>\*space\*</tt>, <tt>\\t</tt>, <tt>\\r</tt>, <tt>\\n</tt> ), ignores back-slashed brackets (<tt>\\\"</tt>, <tt>\'</tt>). If no brackets found, returns a substring not including whitespace characters. Returns NULL, if \c line includes only whilespace characters or no bracket pair found (just one bracket).

For example,
\code
gchar *retstr, *endstr, *s;
s = unbracket_string( "\t one two three ", &retstr, &endstr );
\endcode
will set variables as
\code
s == "one two three "
retstr == "one"
endstr == " two three "
\endcode
and
\code
gchar *retstr, *endstr, *s;
s = unbracket_string( "\t \"one \\'two\\'\" three ", &retstr, &endstr );
\endcode
will set variables as
\code
s == "one \\'two\\'\" three "
retstr == "one \\'two\\'"
endstr == "\" three "
\endcode

\param[in] line Input string, will not be modified.
\param[out] retstr If not NULL, will hold a newly allocated copy of the found substring.
\param[out] endstr If not NULL, will hold a pointer to the second bracket or a whitespace character (if there is no the first bracket).

\return A pointer to the begining of the found substring inside the \c line or NULL.
*/
static gchar*
unbracket_string(
	gchar *line,
	gchar **retstr,
	gchar **endstr )
{
	gchar *s, *end;

	/* ignore whitespace characters */
	for( s = line; strchr( " \t\r\n", (int)s[0] ) != NULL; s++ )
		if( (int)s[0] == (int)'\0' )
			goto failed;

	/* scan for bracket pair */
	switch( (int)s[0] )
	{
		case (int)'"':
		case (int)'\'':
			end = s;
			do
				end = strchr( ++end, (int)s[0] );
			while( end != NULL && end[-1] == (int)'\\' );
			s++;
			break;

		default:
			for( end = s; strchr( " \t\r\n\0", (int)end[0] ) == NULL; end++ );
			break;
	}
	if( end == NULL )
		goto failed;

	if( endstr != NULL )
		*endstr = end;

	if( retstr != NULL )
		*retstr = g_strndup( s, (gsize)end - (gsize)s );

	return s;

failed:
	if( retstr != NULL )
		*retstr = NULL;

	if( endstr != NULL )
		*endstr = NULL;

	return NULL;
}

static glong
receive_response(
	GDataInputStream *data_input,
	DictResponse *resp,
	GError **error )
{
	gchar *line, *message;
	glong code;
	GError *loc_error = NULL;

	g_return_val_if_fail( G_IS_DATA_INPUT_STREAM( data_input ), DICT_CLIENT_ERROR_UNKNOWN_RESPONSE_CODE );

	line = g_data_input_stream_read_line( data_input, NULL, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return DICT_CLIENT_ERROR_UNKNOWN_RESPONSE_CODE;
	}

	code = strtol( line, &message, 10 );
	message++;
	if( resp != NULL && resp->code != NULL )
		*(resp->code) = code;

	switch( code )
	{
		/* simple response */
		case 112:
		case 113:
		case 114:
		case 130:
		case 210:
		case 220:
		case 221:
		case 230:
		case 250:
		case 330:
		case 552:
		case 554:
		case 555:
			if( resp != NULL && resp->message != NULL )
				*(resp->message) = g_strdup( message );
			break;

		/* one numerical argument */
		case 110:
		case 111:
		case 150:
		case 152:
			if( resp != NULL && resp->number != NULL )
			{
				*(resp->number) = strtol( message, &message, 10 );
				message++;
			}
			else
				message = strchr( message, (int)' ' ) + 1;

			if( resp != NULL && resp->message != NULL )
				*(resp->message) = g_strdup( message );
			break;

		/* three textual arguments */
		case 151:
			if( resp != NULL )
			{
				unbracket_string( message, resp->word, &message );
				unbracket_string( ++message, resp->database, &message );
				unbracket_string( ++message, resp->description, NULL );

				/* this case has no additional message */
				if( resp->message != NULL )
					*(resp->message) = NULL;
			}
			break;

		/* error treatment */
		case 420:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_SERVER_TEMPORARY_UNAVAILABLE,
				"Server temporary_unavailable" );
			break;

		case 421:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_SERVER_SHUTTING_DOWN_AT_OPERATOR_REQUEST,
				"Server shutting down at operator request" );
			break;

		case 500:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_SYNTAX_ERROR_COMMAND_NOT_RECOGNIZED,
				"Syntax error command not recognized" );
			break;

		case 501:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_SYNTAX_ERROR_ILLEGAL_PARAMETERS,
				"Syntax error illegal parameters" );
			break;

		case 502:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_COMMAND_NOT_IMPLEMENTED,
				"Command not implemented" );
			break;

		case 503:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_COMMAND_PARAMETER_NOT_IMPLEMENTED,
				"Command parameter not implemented" );
			break;

		case 530:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_ACCESS_DENIED,
				"Access denied" );
			break;

		case 531:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_ACCESS_DENIED_USE_SHOW_INFO_FOR_SERVER_INFORMATION,
				"Access denied, use \"SHOW INFO\" for server information" );
			break;

		case 532:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_ACCESS_DENIED_UNKNOWN_MECHANISM,
				"Access denied, unknown mechanism" );
			break;

		case 550:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_INVALID_DATABASE_USE_SHOW_DB_FOR_LIST_OF_DATABASES,
				"Invalid database use, \"SHOW DB\" for list of databases" );
			break;

		case 551:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_INVALID_STRATEGY_USE_SHOW_STRAT_FOR_A_LIST_OF_STRATEGIES,
				"Invalid strategy, use \"SHOW STRAT\" for a list of strategies" );
			break;

		default:
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_UNKNOWN_RESPONSE_CODE,
				"Unknown response code %ld",
				code );
			break;
	}

	g_free( line );

	return code;
}

static void
send_command(
	GDataOutputStream *data_output,
	const gchar *command,
	GError **error )
{
	GError *loc_error = NULL;

	g_return_if_fail( G_IS_DATA_OUTPUT_STREAM( data_output ) );
	g_return_if_fail( command != NULL );

	g_data_output_stream_put_string( data_output, command, NULL, &loc_error );
	if( loc_error != NULL )
		g_propagate_error( error, loc_error );
}

static gchar*
receive_text(
	GDataInputStream *data_input,
	gsize *length,
	GError **error )
{
	const gchar textend[] = "\r\n.\r\n";
	gchar *text, *buf, *end;
	gsize len, offset;
	GError *loc_error = NULL;

	g_return_val_if_fail( G_IS_DATA_INPUT_STREAM( data_input ), NULL );

	len = 0;
	offset = 0;
	while( TRUE )
	{
		buf = (gchar*)g_buffered_input_stream_peek_buffer( G_BUFFERED_INPUT_STREAM( data_input ), &len );
		end = g_strstr_len( buf + offset, len - offset, textend );

		if( end == NULL )
		{
			/* if buffer is full, increase the buffer size */
			if( g_buffered_input_stream_get_available( G_BUFFERED_INPUT_STREAM( data_input ) ) == g_buffered_input_stream_get_buffer_size( G_BUFFERED_INPUT_STREAM( data_input ) ) )
			{
				g_buffered_input_stream_set_buffer_size( G_BUFFERED_INPUT_STREAM( data_input), g_buffered_input_stream_get_buffer_size( G_BUFFERED_INPUT_STREAM( data_input ) ) + DEFAULT_RECEIVE_TEXT_LEN );

				if( len > sizeof( textend ) - 1 )
					offset = len - ( sizeof( textend ) - 1 );
			}

			/* if there is no data in the stream, set error */
			if( g_buffered_input_stream_fill( G_BUFFERED_INPUT_STREAM( data_input ), -1, NULL, &loc_error ) == 0 )
			{
				g_set_error(
					error,
					DICT_CLIENT_ERROR,
					DICT_CLIENT_ERROR_CAN_NOT_RECOGNIZE_TEXT,
					"Can not recognize text" );
				return NULL;
			}
			else
			{
				if( loc_error != NULL )
				{
					g_propagate_error( error, loc_error );
					return NULL;
				}

				continue;
			}

			/* if text breaker is not found, set error */
			g_set_error(
				error,
				DICT_CLIENT_ERROR,
				DICT_CLIENT_ERROR_CAN_NOT_RECOGNIZE_TEXT,
				"Can not recognize text" );
			return NULL;
		}

		break;
	}

	/* allocate memory for output text */
	len = (gsize)end - (gsize)buf;
	text = g_new( gchar, len + 1 );

	/* copy found text */
	g_input_stream_read( G_INPUT_STREAM( data_input ), text, len, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_free( text );
		g_propagate_error( error, loc_error );
		return NULL;
	}
	text[len] = '\0';

	/* if necessary, return text length */
	if( length != NULL )
		*length = len;

	/* skip text breaker */
	g_input_stream_skip( G_INPUT_STREAM( data_input ), sizeof( textend ) - 1, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_free( text );
		g_propagate_error( error, loc_error );
		return NULL;
	}
	
	return text;
}

static glong
receive_arrays(
	GDataInputStream *data_input,
	GStrv *data,
	GStrv *desc,
	GError **error )
{
	DictResponse resp;
	glong i, number;
	gchar *text, *s;
	GError *loc_error = NULL;

	g_return_val_if_fail( G_IS_DATA_INPUT_STREAM( data_input ), -1 );

	/* try to get number of elements */
	number = 0;
	resp = (DictResponse){NULL,};
	resp.number = &number;
	receive_response( data_input, &resp, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return -1;
	}

	/* number will not change if there is no data */
	if( number == 0 )
	{
		pstrnullv( data );
		pstrnullv( desc );

		return 0;
	}

	/* receive a text holding the list */
	text = receive_text( data_input, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return -1;
	}

	/* allocate arrays */
	pstrallocv_number( data, number + 1 );
	pstrallocv_number( desc, number + 1 );

	/* fill up the arrays */
	if( data != NULL || desc != NULL )
	{
		s = text - 1;
		for( i = 0; i < number; ++i )
		{
			if( data != NULL )
				unbracket_string( ++s, &((*data)[i]), &s );
			else
				unbracket_string( ++s, NULL, &s );

			if( desc != NULL )
				unbracket_string( ++s, &((*desc)[i]), &s );
			else
				unbracket_string( ++s, NULL, &s );
		}
		pstrnullv_index( data, number );
		pstrnullv_index( desc, number );
	}
	g_free( text );

	return number;
}

static gchar*
send_receive_information(
	GDataOutputStream *data_output,
	GDataInputStream *data_input,
	const gchar *command,
	GError **error )
{
	gchar *text;
	GError *loc_error = NULL;

	g_return_val_if_fail( G_IS_DATA_OUTPUT_STREAM( data_output ), NULL );
	g_return_val_if_fail( G_IS_DATA_INPUT_STREAM( data_input ), NULL );
	g_return_val_if_fail( command != NULL, NULL );

	send_command( data_output, command, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return NULL;
	}

	/* receive confirmation */
	receive_response( data_input, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return NULL;
	}

	/* receive information text */
	text = receive_text( data_input, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return NULL;
	}

	/* receive OK status */
	receive_response( data_input, NULL, &loc_error );
	if( loc_error != NULL )
	{
		g_free( text );
		g_propagate_error( error, loc_error );
		return NULL;
	}

	return text;
}

static glong
send_receive_arrays(
	GDataOutputStream *data_output,
	GDataInputStream *data_input,
	const gchar *command,
	GStrv *data,
	GStrv *desc,
	GError **error )
{
	glong number;
	GError *loc_error = NULL;

	g_return_val_if_fail( G_IS_DATA_OUTPUT_STREAM( data_output ), -1 );
	g_return_val_if_fail( G_IS_DATA_INPUT_STREAM( data_input ), -1 );
	g_return_val_if_fail( command != NULL, -1 );

	send_command( data_output, command, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return -1;
	}

	number = receive_arrays( data_input, data, desc, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return -1;
	}

	/* number == 0 means no arrays and no OK status */
	if( number == 0 )
	{
		return 0;
	}

	/* receive OK status */
	receive_response( data_input, NULL, &loc_error );
	if( loc_error != NULL )
	{
		pstrfreev( data );
		pstrfreev( desc );
		g_propagate_error( error, loc_error );
		return -1;
	}

	return number;
}

/**
\anchor dict_client_new
\brief Creates a new DictClient instance.

Use <tt>g_object_unref()</tt> to decrease the reference count of the new instance to 0 and destroys the instance. There is no need to call \ref dict_client_disconnect "dict_client_disconnect()" before.

\return New DictClient instance.
*/
DictClient*
dict_client_new(
	void )
{
	return DICT_CLIENT( g_object_new( G_TYPE_DICT_CLIENT, NULL ) );
}

/**
\anchor dict_client_is_connected
\brief Check whether there was a connection to the server.

This function always returns \c TRUE after successfull call of "dict_client_connect()", and \c FALSE otherwise or after call "dict_client_disconnect()".

\param[in] self A DictClient instance.

\return \c TRUE if there was the successfull connection or \c FALSE otherwise.
*/
gboolean
dict_client_is_connected(
	DictClient *self )
{
	g_return_val_if_fail( DICT_IS_CLIENT( self ), FALSE );
	
	return self->host != NULL;
}

/**
\anchor dict_client_connect
\brief Connects to the server.

Use \ref dict_client_disconnect "dict_client_disconnect()" to disconnect the client from the server or decrease the reference count of the instance to 0 by <tt>g_object_unref()</tt>, that will destroy the instance.

\param[in] self A DictClient instance.
\param[in] host Address of the server (IPv4, IPv6 or resolveable name).
\param[in] port A port number to connect.
\param[in] client_message If not NULL, this message will be sent to the server as a greeting.
\param[out] server_response If not NULL, holds a greeting message from the server.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return \c TRUE on success or \c FALSE on error.
*/
gboolean
dict_client_connect(
	DictClient *self,
	const gchar *host,
	const guint16 port,
	const gchar *client_message,
	gchar **server_response,
	GError **error )
{
	DictResponse resp;
	gchar *command, *message;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), FALSE );
	g_return_val_if_fail( host != NULL, FALSE );

	/* check if there is a connection */
	if( dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_CONNECTION_ALREADY_EXISTS,
			"A connection already exists" );
		return FALSE;
	}

	/* connect to server */
	self->socket = g_socket_client_new();
	self->iostream = G_IO_STREAM( g_socket_client_connect_to_host( self->socket, host, port, NULL, &loc_error ) );
	if( loc_error != NULL )
	{
		g_clear_object( &( self->socket ) );
		g_propagate_error( error, loc_error );
		return FALSE;
	}

	/* make streams */
	self->data_input = g_data_input_stream_new( g_io_stream_get_input_stream( self->iostream ) );
	self->data_output = g_data_output_stream_new( g_io_stream_get_output_stream( self->iostream ) );

	/* \r\n is used for newline */
	g_data_input_stream_set_newline_type( self->data_input, G_DATA_STREAM_NEWLINE_TYPE_CR_LF );
	/* maximum length of a received text is known from the protocol reference */
	g_buffered_input_stream_set_buffer_size( G_BUFFERED_INPUT_STREAM( self->data_input ), DEFAULT_RECEIVE_TEXT_LEN + 1 );

	/* receive response after successful connection */
	resp = (DictResponse){NULL,};
	resp.message = &message;
	receive_response( self->data_input, &resp, &loc_error );
	if( loc_error!= NULL )
	{
		g_propagate_error( error, loc_error );
		goto failed;
	}

	/* introduce client to server */
	if( client_message != NULL )
	{
		command = g_strdup_printf( "CLIENT \"%s\"\r\n", client_message );
		send_command( self->data_output, command, &loc_error );
		g_free( command );
		if( loc_error!= NULL )
		{
			g_propagate_error( error, loc_error );
			goto failed;
		}

		/* receive response after introducing */
		receive_response( self->data_input, NULL, &loc_error );
		if( loc_error!= NULL )
		{
			g_propagate_error( error, loc_error );
			goto failed;
		}
	}

	/* if needed, receive server response text */
	if( server_response != NULL )
		*server_response = message;
	else
		g_free( message );

	/* save successfuly connected host and port */
	self->host = g_strdup( host );
	self->port = port;

	return TRUE;

failed:
	g_clear_object( &self->data_input );
	g_clear_object( &self->data_output );
	g_clear_object( &self->iostream );
	g_clear_object( &self->socket );
	g_free( message );

	return FALSE;
}

/**
\anchor dict_client_disconnect
\brief Breaks a connection to the server.

Any operation with disconnected DictClient will lead to error.

\param[in] self A DictClient instance.
\param[out] server_response If not NULL, holds a farewell message from the server.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return \c TRUE on success or \c FALSE on error.
*/
gboolean
dict_client_disconnect(
	DictClient *self,
	gchar **server_response,
	GError **error )
{
	DictResponse resp;
	gboolean ret = TRUE;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), FALSE );

	if( !dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_NO_CONNECTION,
			"No connection" );
		return FALSE;
	}

	/* send goodbye command to server */
	send_command( self->data_output, "QUIT\r\n", &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		ret = FALSE;
		goto out;
	}

	/* receive farewell response */
	resp = (DictResponse){NULL,};
	resp.message = server_response;
	receive_response( self->data_input, &resp, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		ret = FALSE;
	}

out:
	g_clear_object( &self->data_input );
	g_clear_object( &self->data_output );
	g_clear_object( &self->iostream );
	g_clear_object( &self->socket );
	g_clear_pointer( &self->host, g_free );

	return ret;
}

/**
anchor dict_client_define
\brief Looks up the \c word in the \c database of the server.

If \c database is <tt>!</tt> , then all databases of the server will be scanned until the first match. If \c database is <tt>*</tt> , then all databases of the server will be scaned for all matches. The \c database is searched in the same order as that got by \ref dict_client_show_databases "dict_client_show_databases()". Sizes of \c words, \c databases, \c descriptions and \c definitions are the same.

\param[in] self A \c DictClient instance.
\param[in] database A database to search in, must not be NULL.
\param[in] word A word to search, must not be NULL.
\param[out] words If not NULL, holds an array of words found in the \c databases.
\param[out] databases If not NULL, holds an array of the databases holding the \c words.
\param[out] descriptions If not NULL, holds an array of the descriptions about the \c databases.
\param[out] definitions If not NULL, holds an array of the definitions of the \c words.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return A number of the found definitions or -1 on error.
*/
glong
dict_client_define(
	DictClient *self,
	const gchar *database,
	const gchar *word,
	GStrv *words,
	GStrv *databases,
	GStrv *descriptions,
	GStrv *definitions,
	GError **error )
{
	DictResponse resp;
	gchar *command, *text;
	glong i, number;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), -1 );
	g_return_val_if_fail( database != NULL, -1 );
	g_return_val_if_fail( word != NULL , -1 );

	if( !dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_NO_CONNECTION,
			"No connection" );
		return -1;
	}

	command = g_strdup_printf( "DEFINE \"%s\" \"%s\"\r\n", database, word );
	send_command( self->data_output, command, &loc_error );
	g_free( command );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return -1;
	}

	/* receive number of definitions */
	number = 0;
	resp = (DictResponse){NULL,};
	resp.number = &number;
	receive_response( self->data_input, &resp, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return -1;
	}

	/* number will not change if there is no data */
	/* return empty arrays */
	if( number == 0 )
	{
		pstrnullv( words );
		pstrnullv( databases );
		pstrnullv( descriptions );
		pstrnullv( definitions );

		return 0;
	}

	/* allocate arrays */
	pstrallocv_number( words, number + 1 );
	pstrallocv_number( databases, number + 1 );
	pstrallocv_number( descriptions, number + 1 );
	pstrallocv_number( definitions, number + 1 );

	/* initialize to NULL */
	for( i = 0; i < number + 1; ++i )
	{
		pstrnullv_index( words, i );
		pstrnullv_index( databases, i );
		pstrnullv_index( descriptions, i );
		pstrnullv_index( definitions, i );
	}

	/* receive word, database, description and definitions */
	for( i = 0; i < number; ++i )
	{
		resp = (DictResponse){NULL,};
		if( words != NULL )
			resp.word = &((*words)[i]);
		if( databases != NULL )
			resp.database = &((*databases)[i]);
		if( descriptions != NULL )
			resp.description = &((*descriptions)[i]);

		receive_response( self->data_input, &resp, &loc_error );
		if( loc_error != NULL )
		{
			pstrfreev( words );
			pstrfreev( databases );
			pstrfreev( descriptions );
			pstrfreev( definitions );
			g_propagate_error( error, loc_error );
			return -1;
		}

		text = receive_text( self->data_input, NULL, &loc_error );
		if( loc_error != NULL )
		{
			pstrfreev( words );
			pstrfreev( databases );
			pstrfreev( descriptions );
			pstrfreev( definitions );
			g_propagate_error( error, loc_error );
			return -1;
		}
		if( definitions != NULL )
			(*definitions)[i] = text;
		else
			g_free( text );
	}

	/* receive OK status */
	receive_response( self->data_input, NULL, &loc_error );
	if( loc_error != NULL )
	{
		pstrfreev( words );
		pstrfreev( databases );
		pstrfreev( descriptions );
		pstrfreev( definitions );
		g_propagate_error( error, loc_error );
		return -1;
	}

	return number;
}

/**
\anchor dict_client_match
\brief Trys to match the word in the database with the selected strategy.

This function scans an index of the \c database and returns the \c words array holding words those were found using the \c strategy. Not all databases support all strategies, but all databases must support \c prefix and \c exact. Sizes of returned \c databases and \c words are the same. Use items of \c databases and \c words as pairs.

\param[in] self A \c DictClient instance.
\param[in] database A database to search in, must not be NULL.
\param[in] strategy A strategy to search with, must not be NULL.
\param[in] word A word to search, must not be NULL.
\param[out] databases If not NULL, holds an array of the databases holding the \c words. May be NUll, if no \c word in \c database with \c strategy.
\param[out] words If not NULL, holds an array of words found in the \c databases. May be NUll, if no \c word in \c database with \c strategy.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return A number of the found database-word pairs or -1 on error.
*/
glong
dict_client_match(
	DictClient *self,
	const gchar *database,
	const gchar *strategy,
	const gchar *word,
	GStrv *databases,
	GStrv *words,
	GError **error )
{
	gchar *command;
	glong number;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), -1 );
	g_return_val_if_fail( database != NULL, -1 );
	g_return_val_if_fail( strategy != NULL, -1 );
	g_return_val_if_fail( word != NULL , -1 );

	if( !dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_NO_CONNECTION,
			"No connection" );
		return -1;
	}

	command = g_strdup_printf( "MATCH \"%s\" \"%s\" \"%s\"\r\n", database, strategy,  word );
	number = send_receive_arrays( self->data_output, self->data_input, command, databases, words, &loc_error );
	g_free( command );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return -1;
	}

	return number;
}

/**
\anchor dict_client_show_databases
\brief Recieves an array of currently accessible databases at the server.

\param[in] self A \c DictClient instance.
\param[out] databases If not NULL, holds an array of database names.
\param[out] descriptions If not NULL, holds an array of database descriptions.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return A number of the dictionary databases at the server or -1 on error.
*/
glong
dict_client_show_databases(
	DictClient *self,
	GStrv *databases,
	GStrv *descriptions,
	GError **error )
{
	glong number;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), -1 );

	if( !dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_NO_CONNECTION,
			"No connection" );
		return -1;
	}

	number = send_receive_arrays( self->data_output, self->data_input, "SHOW DATABASES\r\n", databases, descriptions, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return -1;
	}

	return number;
}

/**
\anchor dict_client_show_strategies
\brief Recieves an array of the search strategies supported by the server.

\param[in] self A \c DictClient instance.
\param[out] strategies If not NULL, holds an array of strategy names.
\param[out] descriptions If not NULL, holds an array of strategy descriptions.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return A number of strategies the server can use or -1 on error.
*/
glong
dict_client_show_strategies(
	DictClient *self,
	GStrv *strategies,
	GStrv *descriptions,
	GError **error )
{
	glong number;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), -1 );

	if( !dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_NO_CONNECTION,
			"No connection" );
		return -1;
	}

	number = send_receive_arrays( self->data_output, self->data_input, "SHOW STRATEGIES\r\n", strategies, descriptions, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return -1;
	}

	return number;
}

/**
\anchor dict_client_show_info
\brief Recieves the source, copyright and licensing information about the specified database in free form.

\param[in] self A \c DictClient instance.
\param[in] database Name of the database.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return A newly allocated string or NULL on error.
*/
gchar*
dict_client_show_info(
	DictClient *self,
	const gchar *database,
	GError **error )
{
	gchar *command, *text;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), NULL );
	g_return_val_if_fail( database != NULL, NULL );

	if( !dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_NO_CONNECTION,
			"No connection" );
		return NULL;
	}

	command = g_strdup_printf( "SHOW INFO \"%s\"\r\n", database );
	text = send_receive_information( self->data_output, self->data_input, command, &loc_error );
	g_free( command );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return NULL;
	}

	return text;
}

/**
\anchor dict_client_show_server
\brief Recieves a server information written by the administrator in free form.

\param[in] self A DictClient instance.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return A newly allocated string or NULL on error.
*/
gchar*
dict_client_show_server(
	DictClient *self,
	GError **error )
{
	gchar *text;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), NULL );

	if( !dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_NO_CONNECTION,
			"No connection" );
		return NULL;
	}

	text = send_receive_information( self->data_output, self->data_input, "SHOW SERVER\r\n", &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return NULL;
	}

	return text;
}

/**
\anchor dict_client_status
\brief Recieves some server-specific timing and debugging information about the server in free form.

\param[in] self A DictClient instance.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return A newly allocated string or NULL on error.
*/
gchar*
dict_client_status(
	DictClient *self,
	GError **error )
{
	DictResponse resp;
	gchar *text;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), NULL );

	if( !dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_NO_CONNECTION,
			"No connection" );
		return NULL;
	}

	send_command( self->data_output, "STATUS\r\n", &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return NULL;
	}

	/* status response is a simple line */
	resp = (DictResponse){NULL,};
	resp.message = &text;
	receive_response( self->data_input, &resp, &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return NULL;
	}

	return text;
}

/**
\anchor dict_client_help
\brief Recieves a short summary of commands that are understood by the server.

\param[in] self A DictClient instance.
\param[out] error If not NULL and an error occured, holds a newly allocated GError instance.

\return A newly allocated string or NULL on error.
*/
gchar*
dict_client_help(
	DictClient *self,
	GError **error )
{
	gchar *text;
	GError *loc_error = NULL;

	g_return_val_if_fail( DICT_IS_CLIENT( self ), NULL );

	if( !dict_client_is_connected( self ) )
	{
		g_set_error(
			error,
			DICT_CLIENT_ERROR,
			DICT_CLIENT_ERROR_NO_CONNECTION,
			"No connection" );
		return NULL;
	}

	text = send_receive_information( self->data_output, self->data_input, "HELP\r\n", &loc_error );
	if( loc_error != NULL )
	{
		g_propagate_error( error, loc_error );
		return NULL;
	}

	return text;
}

/**
\anchor dict_client_get_host
\brief Get the host name.

\param[in] self A DictClient instance.

\return A newly allocated string or NULL on error.
*/
gchar*
dict_client_get_host(
	DictClient *self )
{
	g_return_val_if_fail( DICT_IS_CLIENT( self ), NULL );

	return g_strdup( self->host );
}

/**
\anchor dict_client_get_port
\brief Get the port number.

\param[in] self A DictClient instance.

\return An guint16 port number.
*/
guint16
dict_client_get_port(
	DictClient *self )
{
	g_return_val_if_fail( DICT_IS_CLIENT( self ), 0 );

	return self->port;
}

