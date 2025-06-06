/**
\file
\author leonadkr@gmail.com
\brief Header for DictClient class

This header file includes error codes and function primitives.

Typical use of this class:
\code
DictClient *dict_client;
gchar *response;

dict_client = dict_client_new();
dict_client_connect( dict_client, "localhost", 2628, NULL, NULL, NULL );

response = dict_client_show_server( dict_client, NULL );
g_print( "%s\n", response );
g_free( response );

dict_client_disconnect( dict_client, NULL, NULL );

g_object_unref( G_OBJECT( dict_client ) );
\endcode
*/

#ifndef GLIB_DICT_CLIENT_H
#define GLIB_DICT_CLIENT_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

/**
\anchor _DictClientError
\enum _DictClientError
\brief Contains error codes.
*/
enum _DictClientError
{
	DICT_CLIENT_ERROR_SERVER_TEMPORARY_UNAVAILABLE = 420, /**< Server temporary unavailable. */
	DICT_CLIENT_ERROR_SERVER_SHUTTING_DOWN_AT_OPERATOR_REQUEST = 421, /**< Server shutting down at operator request. */
	DICT_CLIENT_ERROR_SYNTAX_ERROR_COMMAND_NOT_RECOGNIZED = 500, /**< Syntax error, command not recognized. */
	DICT_CLIENT_ERROR_SYNTAX_ERROR_ILLEGAL_PARAMETERS = 501, /**< Syntax error, illegal parameters. */
	DICT_CLIENT_ERROR_COMMAND_NOT_IMPLEMENTED = 502, /**< Command not implemented. */
	DICT_CLIENT_ERROR_COMMAND_PARAMETER_NOT_IMPLEMENTED = 503, /**< Command parameter not implemented. */
	DICT_CLIENT_ERROR_ACCESS_DENIED = 530, /**< Access denied. */
	DICT_CLIENT_ERROR_ACCESS_DENIED_USE_SHOW_INFO_FOR_SERVER_INFORMATION = 531, /**< Access denied, use \ref dict_client_show_info "dict_client_show_info()" for server information. */
	DICT_CLIENT_ERROR_ACCESS_DENIED_UNKNOWN_MECHANISM = 532, /**< Access denied, unknown mechanism. */
	DICT_CLIENT_ERROR_INVALID_DATABASE_USE_SHOW_DB_FOR_LIST_OF_DATABASES = 550, /**< Invalid database, use \ref dict_client_show_databases "dict_client_show_databases()" for list of databases. */
	DICT_CLIENT_ERROR_INVALID_STRATEGY_USE_SHOW_STRAT_FOR_A_LIST_OF_STRATEGIES = 551, /**< Invalid strategy, use \ref dict_client_show_strategies "dict_client_show_strategies()" for a list of strategies. */
	DICT_CLIENT_ERROR_CONNECTION_ALREADY_EXISTS = 600, /**< Connection already exists. */
	DICT_CLIENT_ERROR_NO_CONNECTION = 601, /**< No connection. */
	DICT_CLIENT_ERROR_UNKNOWN_RESPONSE_CODE = 700, /**< Unknown response code. */
	DICT_CLIENT_ERROR_CAN_NOT_RECOGNIZE_TEXT = 800, /**< Can not recognize text. */

	N_DICT_CLIENT_ERROR
};
/**
\typedef DictClientError
\brief Synonym for \ref _DictClientError "enum _DictClientError".
*/
typedef enum _DictClientError DictClientError;

#define DICT_CLIENT_ERROR ( dict_client_error_quark() )
GQuark dict_client_error_quark( void );

#define G_TYPE_DICT_CLIENT ( dict_client_get_type() )
G_DECLARE_FINAL_TYPE( DictClient, dict_client, DICT, CLIENT, GObject )

DictClient* dict_client_new( void );
gboolean dict_client_is_connected( DictClient *self );
gboolean dict_client_connect( DictClient *self, const gchar *host, const guint16 port, const gchar *client_message, gchar **server_response, GError **error );
gboolean dict_client_disconnect( DictClient *self, gchar **server_responce, GError **error );
glong dict_client_define( DictClient *self, const gchar *database, const gchar *word, GStrv *words, GStrv *databases, GStrv *descriptions, GStrv *definitions, GError **error );
glong dict_client_match( DictClient *self, const gchar *database, const gchar *strategy, const gchar *word, GStrv *databases, GStrv *words, GError **error );
glong dict_client_show_databases( DictClient *self, GStrv *databases, GStrv *descriptions, GError **error );
glong dict_client_show_strategies( DictClient *self, GStrv *strategies, GStrv *descriptions, GError **error );
gchar* dict_client_show_info( DictClient *self, const gchar *database, GError **error );
gchar* dict_client_show_server( DictClient *self, GError **error );
gchar* dict_client_status( DictClient *self, GError **error );
gchar* dict_client_help( DictClient *self, GError **error );
gchar* dict_client_get_host( DictClient *self );
guint16 dict_client_get_port( DictClient *self );

G_END_DECLS

#endif
