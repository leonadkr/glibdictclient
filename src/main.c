#include "config.h"

#include "lib/glibdictclient.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <locale.h>

#define PROGRAM_APP_SUMMARY "This program uses glibdictclient library for testing purpose. Supported commands: define, match, show_databases, show_strategies, show_info, show_server, status, help."

int
main(
	int argc,
	char *argv[] )
{
	gchar *host = NULL;
	gint port = 2628;
	gchar *strategy = NULL;
	gchar *database = NULL;
	gchar *greeting = NULL;
	gboolean response_set = FALSE;
	const GOptionEntry option_entries[] =
	{
		{ "host", 'h', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &host, "A host address, may include port number. Default is localhost", "HOST" },
		{ "port", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &port, "A port number of the host. Default is 2628.", "PORT" },
		{ "strategy", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &strategy, "A strategy to match word. Check show_strategies for examples. Default is prefix.", "STRATEGY" },
		{ "database", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &database, "A database name. Check show_databases for database names. Default is *.", "DATABASE" },
		{ "greeting", 'g', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &greeting, "An optional message to be sent to the server on connection.", "MESSAGE" },
		{ "response-set", 'r', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &response_set, "If set, response messages from the server on connection and disconnection will be printed.", NULL },
		{ NULL }
	};

	GOptionContext *option_context;
	gchar *help_message, *command, *info, *word, *response;
	DictClient *dc;
	glong i, num;
	GStrv databases, words, strategies, descriptions, definitions;
	gint ret = EXIT_SUCCESS;
	GError *error = NULL;

	setlocale( LC_ALL, "" );

	option_context = g_option_context_new( "COMMAND [WORD]" );
	g_option_context_set_summary( option_context, PROGRAM_APP_SUMMARY );
	g_option_context_set_help_enabled( option_context, TRUE );
	g_option_context_add_main_entries( option_context, option_entries, NULL );

	g_option_context_parse( option_context, &argc, &argv, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		g_option_context_free( option_context );
		return EXIT_FAILURE;
	}

	/* no command -- print help */
	if( argc < 2 )
	{
		help_message = g_option_context_get_help( option_context, FALSE, NULL );
		g_print( "%s", help_message );
		g_free( help_message );
		g_option_context_free( option_context );
		return EXIT_SUCCESS;
	}

	g_option_context_free( option_context );

	/* if options are not set, set to deafults */
	if( host == NULL )
		host = g_strdup( "localhost" );
	if( database == NULL )
		database = g_strdup( "*" );
	if( strategy == NULL )
		strategy = g_strdup( "prefix" );

	/* store command */
	command = argv[1];

	/* connect to the server */
	dc = dict_client_new();
	dict_client_connect( dc, host, port, greeting, &response, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		ret = EXIT_FAILURE;
		goto out;
	}
	if( response_set )
		g_print( "%s\n", response );
	g_free( response );

	/* perform commands */
	if( g_strcmp0( command, "define" ) == 0 )
	{
		if( argc < 3 )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", "No WORD for command %s", command,
				NULL );
			ret = EXIT_FAILURE;
			goto out;
		}
		word = argv[2];

		num = dict_client_define( dc, database, word, &words, &databases, &descriptions, &definitions, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			ret = EXIT_FAILURE;
			goto out;
		}
		for( i = 0; i < num; ++i )
			g_print( "%s\n%s\n%s\n%s\n", words[i], databases[i], descriptions[i], definitions[i] );
		g_strfreev( words );
		g_strfreev( databases );
		g_strfreev( descriptions );
		g_strfreev( definitions );
		goto out;
	}

	if( g_strcmp0( command, "match" ) == 0 )
	{
		if( argc < 3 )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", "No WORD for command %s", command,
				NULL );
			ret = EXIT_FAILURE;
			goto out;
		}
		word = argv[2];

		num = dict_client_match( dc, database, strategy, word, &databases, &words, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			ret = EXIT_FAILURE;
			goto out;
		}
		for( i = 0; i < num; ++i )
			g_print( "%s\t%s\n", databases[i], words[i] );
		g_strfreev( databases );
		g_strfreev( words );
		goto out;
	}

	if( g_strcmp0( command, "show_databases" ) == 0 )
	{
		num = dict_client_show_databases( dc, &databases, &descriptions, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			ret = EXIT_FAILURE;
			goto out;
		}
		for( i = 0; i < num; ++i )
			g_print( "%s\t%s\n", databases[i], descriptions[i] );
		g_strfreev( databases );
		g_strfreev( descriptions );
		goto out;
	}

	if( g_strcmp0( command, "show_strategies" ) == 0 )
	{
		num = dict_client_show_strategies( dc, &strategies, &descriptions, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			ret = EXIT_FAILURE;
			goto out;
		}
		for( i = 0; i < num; ++i )
			g_print( "%s\t%s\n", strategies[i], descriptions[i] );
		g_strfreev( strategies );
		g_strfreev( descriptions );
		goto out;
	}

	if( g_strcmp0( command, "show_info" ) == 0 )
	{
		info = dict_client_show_info( dc, database, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			ret = EXIT_FAILURE;
			goto out;
		}
		g_print( "%s\n", info );
		g_free( info );
		goto out;
	}

	if( g_strcmp0( command, "show_server" ) == 0 )
	{
		info = dict_client_show_server( dc, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			ret = EXIT_FAILURE;
			goto out;
		}
		g_print( "%s\n", info );
		g_free( info );
		goto out;
	}

	if( g_strcmp0( command, "status" ) == 0 )
	{
		info = dict_client_status( dc, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			ret = EXIT_FAILURE;
			goto out;
		}
		g_print( "%s\n", info );
		g_free( info );
		goto out;
	}

	if( g_strcmp0( command, "help" ) == 0 )
	{
		info = dict_client_help( dc, &error );
		if( error != NULL )
		{
			g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				"MESSAGE", error->message,
				NULL );
			g_clear_error( &error );
			ret = EXIT_FAILURE;
			goto out;
		}
		g_print( "%s\n", info );
		g_free( info );
		goto out;
	}

	/* print error, if the command is not supported */
	g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
		"MESSAGE", "Command %s is not supported", command,
		NULL );
	ret = EXIT_FAILURE;

out:
	/* free options */
	g_free( host );
	g_free( database );
	g_free( strategy );

	/* disconnet from the server */
	dict_client_disconnect( dc, &response, &error );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		g_object_unref( G_OBJECT( dc ) );
		return EXIT_FAILURE;
	}
	if( response_set )
		g_print( "%s\n", response );
	g_free( response );
	g_object_unref( G_OBJECT( dc ) );

	return ret;
}

