// include library
#include <stack>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>

///*
//
// src comments: comments in sass syntax (staring with //)
// css comments: multiline comments in css syntax (starting with /*)
//
// KEEP_COMMENT: keep src comments in the resulting css code
// STRIP_COMMENT: strip out all comments (either src or css)
// CONVERT_COMMENT: convert all src comments to css comments
//
//*/

// our own header
#include "sass2scss.h"

// using std::string
using namespace std;

// add namespace for c++
namespace Sass
{

	// return the actual prettify value from options
	#define PRETTIFY(converter) (converter.options - (converter.options & 248))
	// query the options integer to check if the option is enables
	#define KEEP_COMMENT(converter) ((converter.options & SASS2SCSS_KEEP_COMMENT) == SASS2SCSS_KEEP_COMMENT)
	#define STRIP_COMMENT(converter) ((converter.options & SASS2SCSS_STRIP_COMMENT) == SASS2SCSS_STRIP_COMMENT)
	#define CONVERT_COMMENT(converter) ((converter.options & SASS2SCSS_CONVERT_COMMENT) == SASS2SCSS_CONVERT_COMMENT)

	// some makros to access the indentation stack
	#define INDENT(converter) (converter.indents.top())

	// some makros to query comment parser status
	#define IS_PARSING(converter) (converter.comment == "")
	#define IS_COMMENT(converter) (converter.comment != "")
	#define IS_SRC_COMMENT(converter) (converter.comment == "//" && ! CONVERT_COMMENT(converter))
	#define IS_CSS_COMMENT(converter) (converter.comment == "/*" || (converter.comment == "//" && CONVERT_COMMENT(converter)))

	// pretty printer helper function
	static string closer (const converter& converter)
	{
		return PRETTIFY(converter) == 0 ? " }" :
		     PRETTIFY(converter) <= 1 ? " }" :
		       "\n" + INDENT(converter) + "}";
	}

	// pretty printer helper function
	static string opener (const converter& converter)
	{
		return PRETTIFY(converter) == 0 ? " { " :
		     PRETTIFY(converter) <= 2 ? " {" :
		       "\n" + INDENT(converter) + "{";
	}

	// check if there is some char data
	// will ignore everything in comments
	static bool hasCharData (string& sass)
	{

		size_t col_pos = 0;

		while (true)
		{

			// try to find some meaningfull char
			col_pos = sass.find_first_not_of(" \t\n\v\f\r", col_pos);

			// there was no meaningfull char found
			if (col_pos == string::npos) return false;

			// found a multiline comment opener
			if (sass.substr(col_pos, 2) == "/*")
			{
				// find the multiline comment closer
				col_pos = sass.find("*/", col_pos);
				// maybe we did not find the closer here
				if (col_pos == string::npos) return false;
				// skip closer
				col_pos += 2;
			}
			else
			{
				return true;
			}

		}

	}
	// EO hasCharData

	// find src comment opener
	// correctly skips quoted strings
	static size_t findCommentOpener (string& sass)
	{

		size_t col_pos = 0;
		bool quoted = false;
		bool comment = false;

		while (col_pos != string::npos)
		{

			// process all interesting chars
			col_pos = sass.find_first_of("\"/\\*", col_pos);

			// assertion for valid result
			if (col_pos != string::npos)
			{

				if (sass.at(col_pos) == '\"')
				{
					// invert quote bool
					quoted = !quoted;
				}
				else if (sass.at(col_pos) == '/')
				{
					if (col_pos > 0 && sass.at(col_pos - 1) == '*')
					{
						comment = false;
					}
					// next needs to be a slash too
					else if (col_pos > 0 && sass.at(col_pos - 1) == '/')
					{
						// only found if not in quote or comment
						if (!quoted && !comment) return col_pos - 1;
					}
				}
				else if (sass.at(col_pos) == '\\')
				{
					// skip next char if in quote
					if (quoted) col_pos ++;
				}
				// this might be a comment opener
				else if (sass.at(col_pos) == '*')
				{
					// opening a multiline comment
					if (col_pos > 0 && sass.at(col_pos - 1) == '/')
					{
						// we are now in a comment
						if (!quoted) comment = true;
					}
				}

				// skip char
				col_pos ++;

			}

		}
		// EO while

		return col_pos;

	}
	// EO findCommentOpener

	// remove multiline comments from sass string
	// correctly skips quoted strings
	static string removeMultilineComment (string &sass)
	{

		string clean = "";
		size_t col_pos = 0;
		size_t open_pos = 0;
		size_t close_pos = 0;
		bool quoted = false;
		bool comment = false;

		// process sass til string end
		while (col_pos != string::npos)
		{

			// process all interesting chars
			col_pos = sass.find_first_of("\"/\\*", col_pos);

			// assertion for valid result
			if (col_pos != string::npos)
			{

				// found quoted string delimiter
				if (sass.at(col_pos) == '\"')
				{
					if (!comment) quoted = !quoted;
				}
				// found possible comment closer
				else if (sass.at(col_pos) == '/')
				{
					// look back to see if it is actually a closer
					if (comment && col_pos > 0 && sass.at(col_pos - 1) == '*')
					{
						close_pos = col_pos + 1; comment = false;
					}
				}
				else if (sass.at(col_pos) == '\\')
				{
					// skip escaped char
					if (quoted) col_pos ++;
				}
				// this might be a comment opener
				else if (sass.at(col_pos) == '*')
				{
					// look back to see if it is actually an opener
					if (!quoted && col_pos > 0 && sass.at(col_pos - 1) == '/')
					{
						comment = true; open_pos = col_pos - 1;
						clean += sass.substr(close_pos, open_pos - close_pos);
					}
				}

				// skip char
				col_pos ++;

			}

		}
		// EO while

		// add final parts (add half open comment text)
		if (comment) clean += sass.substr(open_pos);
		else clean += sass.substr(close_pos);

		// return string
		return clean;

	}
	// EO removeMultilineComment

	// right trim a given string
	string rtrim(const string &sass)
	{
		string trimmed = sass;
		size_t pos_ws = trimmed.find_last_not_of(" \t\n\v\f\r");
		if (pos_ws != string::npos)
		{ trimmed.erase(pos_ws + 1); }
		else { trimmed.clear(); }
		return trimmed;
	}
	// EO rtrim

	// flush whitespace and print additional text, but
	// only print additional chars and buffer whitespace
	string flush (string& sass, converter& converter)
	{

		// return flushed
		string scss = "";

		// print whitespace buffer
		scss += PRETTIFY(converter) > 0 ?
		        converter.whitespace : "";
		// reset whitespace buffer
		converter.whitespace = "";

		// remove possible newlines from string
		size_t pos_right = sass.find_last_not_of("\n\r");
		if (pos_right == string::npos) return scss;

		// get the linefeeds from the string
		string lfs = sass.substr(pos_right + 1);
		sass = sass.substr(0, pos_right + 1);

		// find some source comment opener
		size_t comment_pos = findCommentOpener(sass);
		// check if there was a source comment
		if (comment_pos != string::npos)
		{
			// convert comment (but only outside other coments)
			if (CONVERT_COMMENT(converter) && !IS_COMMENT(converter))
			{
				// convert to multiline comment
				sass.at(comment_pos + 1) = '*';
				// add comment node to the whitespace
				sass += " */";
			}
			// not at line start
			if (comment_pos > 0)
			{
				// also include whitespace before the actual comment opener
				size_t ws_pos = sass.find_last_not_of(" \t\n\v\f\r", comment_pos - 1);
				comment_pos = ws_pos == string::npos ? 0 : ws_pos + 1;
			}
			if (!STRIP_COMMENT(converter))
			{
				// add comment node to the whitespace
				converter.whitespace += sass.substr(comment_pos);
			}
			else
			{
				// sass = removeMultilineComments(sass);
			}
			// update the actual sass code
			sass = sass.substr(0, comment_pos);
		}

		// add newline as getline discharged it
		converter.whitespace += lfs + "\n";

		// maybe remove any leading whitespace
		if (PRETTIFY(converter) == 0)
		{
			// remove leading whitespace and update string
			size_t pos_left = sass.find_first_not_of(" \t\n\v\f\r");
			if (pos_left != string::npos) sass = sass.substr(pos_left);
		}

		// add flushed data
		scss += sass;

		// return string
		return scss;

	}
	// EO flush

	// process a line of the sass text
	string process (string& sass, converter& converter)
	{

		// resulting string
		string scss = "";

		// strip multi line comments
		if (STRIP_COMMENT(converter))
		{
			sass = removeMultilineComment(sass);
		}

		// right trim input
		sass = rtrim(sass);

		// get postion of first meaningfull character in string
		size_t pos_left = sass.find_first_not_of(" \t\n\v\f\r");

		// special case for final run
		if (converter.end_of_file) pos_left = 0;

		// maybe has only whitespace
		if (pos_left == string::npos)
		{
			// just add complete whitespace
			converter.whitespace += sass + "\n";
		}
		// have meaningfull first char
		else
		{

			// extract and store indentation string
			string indent = sass.substr(0, pos_left);

			// check if current line starts a comment
			string open = sass.substr(pos_left, 2);

			// line has less or same indentation
			// finalize previous open parser context
			if (indent.length() <= INDENT(converter).length())
			{

				// close multilinie comment
				if (IS_CSS_COMMENT(converter))
				{
					// check if comments will be stripped anyway
					if (!STRIP_COMMENT(converter)) scss += " */";
				}
				// close src comment comment
				else if (IS_SRC_COMMENT(converter))
				{
					// add a newline to avoid closer on same line
					// this would put the bracket in the comment node
					// no longer needed since we parse them correctly
					// if (KEEP_COMMENT(converter)) scss += "\n";
				}
				// close css properties
				else if (converter.property)
				{
					// add closer unless in concat mode
					if (!converter.comma)
					{
						// if there was no colon we have a selector
						// looks like there were no inner properties
						if (converter.selector) scss += " {}";
						// add final semicolon
						else scss += ";";
					}
				}

				// reset comment state
				converter.comment = "";

			}

			// make sure we close every "higher" block
			while (indent.length() < INDENT(converter).length())
			{
				// pop stacked context
				converter.indents.pop();
				// print close bracket
				if (IS_PARSING(converter))
				{ scss += closer(converter); }
				else { scss += " */"; }
				// reset comment state
				converter.comment = "";
			}

			// reset converter state
			converter.selector = false;

			// check if we have sass property syntax
			if (sass.substr(pos_left, 1) == ":")
			{
				// get postion of first whitespace char
				size_t pos_wspace = sass.find_first_of(" \t\n\v\f\r", pos_left);
				// assertion check for valid result
				if (pos_wspace != string::npos)
				{
					// get position of the first real property value char
					// pseudo selectors get this far, but have no actual value
					size_t pos_value =  sass.find_first_not_of(" \t\n\v\f\r", pos_wspace);
					// assertion check for valid result
					if (pos_value != string::npos)
					{
						// create new string by interchanging the colon sign for property and value
						sass = indent + sass.substr(pos_left + 1, pos_wspace - pos_left - 1) + ":" + sass.substr(pos_wspace);
					}
				}

				// try to find a colon in the current line, but only ...
				size_t pos_colon = sass.find_first_not_of(":", pos_left);
				// assertion for valid result
				if (pos_colon != string::npos)
				{
					// ... after the first word (skip begining colons)
					pos_colon = sass.find_first_of(":", pos_colon);
					// it is a selector if there was no colon found
					converter.selector = pos_colon == string::npos;
				}

			}

			// replace some specific sass shorthand directives (if not fallowed by a white space character)
			else if (sass.substr(pos_left, 1) == "=" && sass.find_first_of(" \t\n\v\f\r", pos_left) != pos_left + 1)
			{ sass = indent + "@mixin " + sass.substr(pos_left + 1); }
			else if (sass.substr(pos_left, 1) == "+" && sass.find_first_of(" \t\n\v\f\r", pos_left) != pos_left + 1)
			{ sass = indent + "@include " + sass.substr(pos_left + 1); }

			// add quotes for import if needed
			else if (sass.substr(pos_left, 7) == "@import")
			{
				// get positions for the actual import url
				size_t pos_import = sass.find_first_of(" \t\n\v\f\r", pos_left + 7);
				size_t pos_quote = sass.find_first_not_of(" \t\n\v\f\r", pos_import);
				// check if the url is quoted
				if (sass.substr(pos_quote, 1) != "\"")
				{
					// get position of the last char on the line
					size_t pos_end = sass.find_last_not_of(" \t\n\v\f\r");
					// assertion check for valid result
					if (pos_end != string::npos)
					{
						// add quotes around the full line after the import statement
						sass = sass.substr(0, pos_quote) + "\"" + sass.substr(pos_quote, pos_end - pos_quote + 1) + "\"";
					}
				}

			}
			else if (
				sass.substr(pos_left, 7) != "@return" &&
				sass.substr(pos_left, 7) != "@extend" &&
				sass.substr(pos_left, 8) != "@content"
			) {

				// try to find a colon in the current line, but only ...
				size_t pos_colon = sass.find_first_not_of(":", pos_left);
				// assertion for valid result
				if (pos_colon != string::npos)
				{
					// ... after the first word (skip begining colons)
					pos_colon = sass.find_first_of(":", pos_colon);
					// it is a selector if there was no colon found
					converter.selector = pos_colon == string::npos;
				}

			}

			// current line has more indentation
			if (indent.length() >= INDENT(converter).length())
			{
				// not in comment mode
				if (IS_PARSING(converter))
				{
					// has meaningfull chars
					if (hasCharData(sass))
					{
						// is probably a property
						// also true for selectors
						converter.property = true;
					}
				}
			}
			// current line has more indentation
			if (indent.length() > INDENT(converter).length())
			{
				// not in comment mode
				if (IS_PARSING(converter))
				{
					// had meaningfull chars
					if (converter.property)
					{
						// print block opener
						scss += opener(converter);
						// push new stack context
						converter.indents.push("");
						// store block indentation
						INDENT(converter) = indent;
					}
				}
				// is and will be a src comment
				else if (!IS_CSS_COMMENT(converter))
				{
					// scss does not allow multiline src comments
					// therefore add forward slashes to all lines
					sass.at(INDENT(converter).length()+0) = '/';
					// there is an edge case here if indentation
					// is minimal (will overwrite the fist char)
					sass.at(INDENT(converter).length()+1) = '/';
					// could code around that, but I dont' think
					// this will ever be the cause for any trouble
				}
			}

			// line is opening a new comment
			if (open == "/*" || open == "//")
			{
				// reset the property state
				converter.property = false;
				// close previous comment
				if (IS_CSS_COMMENT(converter) && open != "")
				{
					if (!STRIP_COMMENT(converter) && !CONVERT_COMMENT(converter)) scss += " */";
				}
				// force single line comments
				// into a correct css comment
				if (CONVERT_COMMENT(converter))
				{
					if (IS_PARSING(converter))
					{ sass.at(pos_left + 1) = '*'; }
				}
				// set comment flag
				converter.comment = open;

			}

			// flush data only under certain conditions
			if (!(
				// strip css and src comments if option is set
				(IS_COMMENT(converter) && STRIP_COMMENT(converter)) ||
				// strip src comment even if strip option is not set
				// but only if the keep src comment option is not set
				(IS_SRC_COMMENT(converter) && ! KEEP_COMMENT(converter))
			))
			{
				// flush data and buffer whitespace
				scss += flush(sass, converter);
			}

			// get postion of last meaningfull char
			size_t pos_right = sass.find_last_not_of(" \t\n\v\f\r");

			// check for invalid result
			if (pos_right != string::npos)
			{

				// get the last meaningfull char
				string close = sass.substr(pos_right, 1);

				// check if next line should be concatenated (list mode)
				converter.comma = IS_PARSING(converter) && close == ",";

				// check if we have more than
				// one meaningfull char
				if (pos_right > 0)
				{

					// get the last two chars from string
					string close = sass.substr(pos_right - 1, 2);
					// update parser status for expicitly closed comment
					if (close == "*/") converter.comment = "";

				}

			}
			// EO have meaningfull chars from end

		}
		// EO have meaningfull chars from start

		// return scss
		return scss;

	}
	// EO process

	// the main converter function for c++
	char* sass2scss (const string sass, const int options)
	{

		// local variables
		string line;
		string scss = "";
		const char delim = '\n';
		stringstream stream(sass);

		// create converter variable
		converter converter;
		// initialise all options
		converter.comma = false;
		converter.property = false;
		converter.end_of_file = false;
		converter.comment = "";
		converter.whitespace = "";
		converter.indents.push("");
		converter.options = options;

		// read line by line and process them
		while(std::getline(stream, line, delim))
		{ scss += process(line, converter); }

		// create mutable string
		string closer = "";
		// set the end of file flag
		converter.end_of_file = true;
		// process to close all open blocks
		scss += process(closer, converter);

		// allocate new memory on the heap
		// caller has to free it after use
		char * cstr = new char [scss.length()+1];
		// create a copy of the string
		strcpy (cstr, scss.c_str());
		// return pointer
		return &cstr[0];

	}
	// EO sass2scss

}
// EO namespace

// implement for c
extern "C"
{

	char* sass2scss (const char* sass, const int options)
	{
		return Sass::sass2scss(sass, options);
	}

}
