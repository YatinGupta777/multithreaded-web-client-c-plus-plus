// HTMLParserTest.cpp
// Example program showing how to use the parser
// CSCE 463 sample code
//
#include "pch.h"

int html_parser(char* html_code, char* link, int html_content_length, HTMLParserBase*&parser)
{
	// create new parser object
	int nLinks;

	char *linkBuffer = parser->Parse (html_code, html_content_length, link, (int)strlen(link), &nLinks);

	// check for errors indicated by negative values
	if (nLinks < 0)
		nLinks = 0;

	return nLinks;
}

