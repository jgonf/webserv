#include "../includes/webserv.hpp"

// source : https://www.tutorialspoint.com/http/http_responses.htm

template <typename T>
string NumberToString(T Number)
{
	std::ostringstream ss;
	ss << Number;
	return ss.str();
}

void add_status_line(string &status_line, string code)
{
	map_str_str statusMsg = statusCodes();
	status_line += code;
	status_line += " ";
	status_line += statusMsg[code];
	status_line += "\n";
}

void add_header(string &headers, string header, string value)
{
	headers += header;
	headers += value;
	headers += "\n";
}

string get_current_date()
{
	//from https://stackoverflow.com/questions/2408976/struct-timeval-to-printable-format
	struct timeval tv;
	time_t nowtime;
	struct tm *nowtm;
	char tmbuf[64];

	gettimeofday(&tv, NULL);
	nowtime = tv.tv_sec;
	nowtm = localtime(&nowtime); // in <ctime> so authorized
	strftime(tmbuf, sizeof tmbuf, "%a, %d %b %Y %H:%M:%S GMT", nowtm);
	//tmbuf should look like "Sun, 06 Nov 1994 08:49:37 GMT"

	return string(tmbuf);
}

string get_last_modified(const char *path)
{
	struct stat stat_struct;
	time_t last_modification;
	struct tm *mtime;
	char tmbuf[64];

	if (stat(path, &stat_struct) != 0)
		std::cout << strerror(errno) << " for path " << path << std::endl;
	; //gerer les cas d'erreur ?
	last_modification = stat_struct.st_mtime;
	mtime = localtime(&last_modification);
	strftime(tmbuf, sizeof tmbuf, "%a, %d %b %Y %H:%M:%S GMT", mtime);

	/* 	Quizas gerer ca (via strptime ?):
	 An origin server with a clock MUST NOT send a Last-Modified date that
   is later than the server's time of message origination (Date).  If
   the last modification time is derived from implementation-specific
   metadata that evaluates to some time in the future, according to the
   origin server's clock, then the origin server MUST replace that value
   with the message origination date.  This prevents a future
   modification date from having an adverse impact on cache validation. 

   An origin server without a clock MUST NOT assign Last-Modified values
   to a response unless these values were associated with the resource
   by some other system or user with a reliable clock.   */

	return string(tmbuf);
}

off_t get_file_size(const char *path)
{
	struct stat stat_struct;

	if (stat(path, &stat_struct) != 0)
	{
		std::cerr << strerror(errno) << " for path " << path << std::endl; //pertinence ?
		return -1;
	}
	else
		return stat_struct.st_size;
}

std::vector<string> convert_CGI_string_to_vector(string CGI_string)
{
	std::vector<string> pairs;

	char *to_split = (char *)CGI_string.c_str();
	char *ret;
	ret = strtok(to_split, "&");
	while (ret != NULL)
	{
		pairs.push_back(string(ret));
		ret = strtok(NULL, "&");
	}
	return pairs;
}

char *clean_CGI_env_token(string token) //a voir si on fait plus propre ou osef= a voir si on fouille un peu dans la syntaxe des bodys envoyes par POST ou pas
{
	char *shiny_token;

	if ((shiny_token = (char *)malloc(sizeof(char) * (token.size() + 1))) == NULL)
	{
		std::cerr << "Failed allocating memory for a brand new shiny CGI token\n";
		return NULL;
	}
	shiny_token[token.size()] = '\0';
	strcpy(shiny_token, token.c_str());
	for (size_t i = 0; i < strlen(shiny_token); i++)
	{
		if (shiny_token[i] == '+')
			shiny_token[i] = ' ';
	}
	return shiny_token;
}

char **convert_CGI_vector_to_CGI_env(std::vector<string> CGI_vector)
{
	char **CGI_env = NULL;

	if ((CGI_env = (char **)malloc(sizeof(char *) * (CGI_vector.size() + 1))) == NULL)
	{
		std::cerr << "Failed allocating memory for CGI env\n";
		return NULL;
	}
	CGI_env[CGI_vector.size()] = NULL;
	for (size_t i = 0; i < CGI_vector.size(); i++)
		CGI_env[i] = clean_CGI_env_token(CGI_vector[i]); //achtung malloc
	return CGI_env;
}

void free_CGI_env(char **CGI_env)
{
	size_t i = 0;
	while (CGI_env && CGI_env[i])
	{
		free(CGI_env[i]);
		CGI_env[i] = NULL;
		i++;
	}
	if (CGI_env)
		free(CGI_env);
	CGI_env = NULL;
}

size_t response_to_POST(Request &request, string &response)
{
	string request_body = request.get_body();
	std::vector<string> CGI_vector = convert_CGI_string_to_vector(request_body);
	char **CGI_env = convert_CGI_vector_to_CGI_env(CGI_vector); //double malloc

	//rajouter if on est bien dans un cgi

	int link[2];
	char cgi_response[8000]; // TAILLE RANDOM ICI, FAUDRA METTRE UN VRAI TRUC /probablement client body size
	pipe(link);
	if (fork() == 0)
	{
		dup2(link[1], STDOUT_FILENO);
		close(link[0]);
		close(link[1]);
		char **input = (char **)malloc(sizeof(char *) * 3);

		input[0] = strdup("/usr/bin/php-cgi");
		input[1] = strdup("./srcs/cgi/post.php"); //a modifier
		input[2] = NULL;
		execve("/usr/bin/php-cgi", input, CGI_env);
		free(input[0]);
		free(input[1]);
		free(input);
		free_CGI_env(CGI_env);
		exit(EXIT_FAILURE);
	}
	else
	{
		close(link[1]);
		read(link[0], cgi_response, sizeof(cgi_response));
		string cgi_str(cgi_response);
		add_status_line(response, CREATED); //a verifier
		char separator[5] = {13, 10, 13, 10, 0};
		size_t pos = cgi_str.find(separator) + 4;
		string body = string(cgi_str, pos);
		char *size_itoa;
		size_itoa = (char *)NumberToString(body.size()).c_str();
		add_header(response, "Date: ", get_current_date());	
		add_header(response, "Content-Length: ", string(size_itoa));
		size_t begin = cgi_str.find("Content-type: ");
		size_t end = string(cgi_str, begin + 14).find('\n' | ';');
		if (end != string::npos && begin != string::npos)
			add_header(response, "Content-Type: ", string(cgi_str, begin + 14, end));
		response += '\n';
		response += body;
	}
	return 42;
}


size_t default_response(string &response, string code)
{
	add_status_line(response, code);
	add_header(response, "Date: ", get_current_date());
	return response.size();
}

size_t response_to_GET_or_HEAD(Request &request, string &response)
{
	std::cout << "response to get" << std::endl;
	off_t file_size;
	if (request.get_path() == "srcs/cgi/postform.php" && request.get_query_string() != string())//remplacer par Celia
	{
	std::cout << "if" << std::endl;
		std::cerr << "getting GET cgi_request\n";
		Request newRequest = Request(request, request.get_query_string());
		std::cout << newRequest << std::endl;
		return response_to_POST(newRequest, response);
	}
	else if ((file_size = get_file_size(request.get_path().c_str())) >= 0)
	{
	std::cout << "else if" << std::endl;
		//opening file
		std::ifstream stream;
		stream.open(request.get_path().c_str(), std::ifstream::binary);

		//status line
		add_status_line(response, OK); //200

		//Adding headers
		char *size_itoa;
		size_itoa = (char *)NumberToString(file_size).c_str();
		add_header(response, "Date: ", get_current_date());
		add_header(response, "Last-Modified: ", get_last_modified(request.get_path().c_str()));
		add_header(response, "Server: ", "Our webserv"); //choix statique a confirmer
		if (request.get_method() == "GET")		// Ajout du body
		{
			char *buffer = new char[file_size];
			std::ifstream stream;
			stream.open(request.get_path().c_str(), std::ifstream::binary);
			stream.read(buffer, sizeof(char) * file_size);
			add_header(response, "Content-Length: ", string(size_itoa));
			response += "\n";
			response += string(buffer);
			delete[] buffer;
		}
		stream.close();
	}
	else //stat renvoie -1 == 404 Not Found ou voir en fonction de errno ?
	{
		std::cout << "else" << std::endl;
		return default_response(response, NOT_FOUND);
	}
	return 42;
}

Server *choose_server(Request &request, std::vector<Server> &servers)
{
	bool port_found = false;
	Server *chosen_server = new Server(servers[0]);
	for (std::vector<Server>::iterator it = servers.begin(); it != servers.end(); ++it)
	{
		if (it->get_port_str() == request.get_port())
		{
			if (port_found == false)
			{
				port_found = true;
				delete chosen_server;
				chosen_server = new Server(*it);
			}
			if (it->get_host() == request.get_host())
			{
				delete chosen_server;
				chosen_server = new Server(*it);
				return (chosen_server);
			}
		}
	}
	return chosen_server;
}

Location choose_location(string path, vec_location locations)
{
	string to_match = string("/") + path;
	while (to_match != string())
	{
		for (vec_location::iterator it = locations.begin(); it != locations.end(); it++)
		{
			if (to_match.find(it->get_path()) == 0) //=first matching
				return (*it);
		}
		to_match = string(to_match, 0, to_match.find_last_of('/'));
	}
	return Location();
}

size_t redirected_response(string &response, pair_str_str redirect)
{
	add_status_line(response, redirect.first);
	add_header(response, "Date: ", get_current_date());
	add_header(response, "Location: ", redirect.second);
	return response.size();
}

bool redirection_found(Location &location)
{
	if (location.is_empty() || location.get_redirect() == pair_str_str(string(), string()))
		return false;
	else
		return true;
}

size_t build_response(Request &request, string &response, std::vector<Server> &servers)
{
	if (request.is_bad_request())
		return default_response(response, BAD_REQUEST); //400
	Server *server = choose_server(request, servers);
	// std::cout << "\n----SELECTED SERVER IS:"
	// 		  << server;
	Location location = choose_location(request.get_path(), server->get_locations());
	// std::cout << "----SELECTED LOCATION IS:"
	// 		  << location;
	if (!location.method_is_allowed(request.get_method()))
	{
		delete server;
		return default_response(response, NOT_ALLOWED); //ou 403?
	}
	if (location.is_empty() || !location.root_is_set())
		request.append_root_to_path(server->get_root());
	else
		request.append_root_to_path(location.get_root());
	delete server;
	std::cout << "avant test" << std::endl;
	std::cout << "apres test" << std::endl;
	if (redirection_found(location))
		return redirected_response(response, location.get_redirect());
	else if (request.get_method() == "GET" || request.get_method() == "HEAD")
	{
		std::cout << "test" << std::endl;
		return response_to_GET_or_HEAD(request, response);
	}
	else if (request.get_method() == "POST")
		return response_to_POST(request, response);
	else if (request.get_method() == "DELETE")
		return 42;									//a implementer
	return default_response(response, NOT_ALLOWED); //405
}

/* response headers:
"Allow" https://datatracker.ietf.org/doc/html/rfc7231#section-7.4.1 obligatoire pour reponse 405
"Content-Language" //osef
"Content-Length" https://datatracker.ietf.org/doc/html/rfc7230#section-3.3.2
"Content-Location"	//osef
"Content-Type"
"Date" https://datatracker.ietf.org/doc/html/rfc7231#section-7.1.1.2 
"Last-Modified" https://datatracker.ietf.org/doc/html/rfc7232#section-2.2
"Location" https://datatracker.ietf.org/doc/html/rfc7231#section-7.1.2 utilise en cas de creation (201) ou de redirection (3xx)
"Retry-After" //osef
"Server" https://datatracker.ietf.org/doc/html/rfc7231#section-7.4.2
"Transfer-Encoding" osef ? https://datatracker.ietf.org/doc/html/rfc7230#section-3.3.1
"WWW-Authenticate" osef ?
 */
