#pragma once

#include "sql_mm.h"

struct MySQLConnectionInfo
{
    const char *host;
    const char *user;
    const char *pass;
    const char *database;
    int port = 3306;
    int timeout = 60;
};

class IMySQLConnection : public ISQLConnection
{
};

class IMySQLClient
{
public:
    virtual IMySQLConnection *CreateMySQLConnection(MySQLConnectionInfo info) = 0;
};
