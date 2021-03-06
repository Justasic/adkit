#pragma once
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <map>
#include <vector>
#include <string>
#include <cstring>


// Mysql
#include <mysql/my_global.h>
#include <mysql/mysql.h>

#if 0
typedef struct MySQL_Result_s {
	int fields;
	std::map<int, MYSQL_ROW> rows;
} MySQL_Result;
#endif
//                 field/col    value
typedef std::map<std::string, std::string> MySQL_Row;
// vector of rows
typedef std::vector<MySQL_Row> MySQL_Result;

class MySQLException : public std::exception
{
	const char *str;
public:
	MySQLException(const std::string &mstr)
	{
		// we're crashing, I honestly have bigger issues than memleaks right now.
		this->str = strdup(mstr.c_str());
	}

	~MySQLException()
	{
		// albeit this is very bad, I won't concern myself with it right now.
		delete this->str;
	}

	const char *what() const noexcept
	{
		return this->str;
	}
};

class MySQL
{
	// MySQL connection object
	MYSQL *con;

	// As much as I hate storing these, we must to reconnect on timeouts.
	std::string user, pass, host, db;

	// MySQL server's port.
	short int port;

	// Used to connect to the database
	bool DoConnection();
public:

	MySQL(const std::string &host, const std::string &user, const std::string &pass, const std::string &db, short);
	~MySQL();

	// Run a query
	MySQL_Result Query(const std::string &query);

	// Check the connection to the database.
	bool CheckConnection();

	// Escape a string
	std::string Escape(const std::string &str);
};
