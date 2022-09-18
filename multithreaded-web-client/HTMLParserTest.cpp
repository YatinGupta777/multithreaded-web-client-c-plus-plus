// HTMLParserTest.cpp
// Example program showing how to use the parser
// CSCE 463 sample code
//
#include "pch.h"

int html_parser(char* html_code, char* link, int html_content_length, HTMLParserBase*&parser, int& tamu_links)
{
	// create new parser object
	int nLinks;

	char *linkBuffer = parser->Parse (html_code, html_content_length, link, (int)strlen(link), &nLinks);

	// check for errors indicated by negative values
	if (nLinks < 0)
		nLinks = 0;

	for (int i = 0; i < nLinks; i++)
	{
		//tamu.edu
		if (strlen(linkBuffer) >= 8 && strcmp(linkBuffer + strlen(linkBuffer) - 8, "tamu.edu") == 0) tamu_links++;
		//tamu.edu/
		else if (strlen(linkBuffer) >= 9 && strcmp(linkBuffer + strlen(linkBuffer) - 9, "tamu.edu/") == 0) tamu_links++;
		linkBuffer += strlen(linkBuffer) + 1;
	}
	
	return nLinks;
}

