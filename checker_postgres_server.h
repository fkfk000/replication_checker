#ifndef POSTGRES_SERVER_H
#define POSTGRES_SERVER_H

#include "util.h"

#include <unordered_map>
#include <vector>

// used in relation information
struct columnInfo
{
    std::int8_t keyFlag;
    std::string columnName;
    Oid columnType;
    std::int32_t atttypmod;
};

// used in checking TupleData.
// we only support string now.
struct columnData
{
    char type;
    int len;
    std::string data;
};

struct relationInfo
{
    Oid oid;
    std::string nameSpace;
    std::string relationName;
    char replicaIdentity;
    int columnCount;
    std::vector<struct columnInfo> cloumnInfos;
};

// used for checking TupleData
struct rowData
{
    int columnCount;
    char type;
    std::vector<struct columnData> data;
    int len; // when returned, we need this information to continue processing.
};

class PostgresServer
{
private:
    std::shared_ptr<PGconn> conn;
    int serverVersion;
    char *copyBuf = nullptr;
    std::unordered_map<Oid, struct relationInfo> relationMap;
    bool sendFeedback();
    void checkFeedback(); // check if we need to send feedback. If we need, send it.
    void process_keepalived_message();
    void porcess_relation_message(char *buf);
    void process_begin_message(char *buf);
    void process_insert_message(char *buf);
    rowData process_tupledata(char *buf, int len);
    void process_commit_message(char *buf);
    void porcess_delete_message(char *buf);
    void process_update_message(char *buf);
    void process_stream_start(char *buf);
    void process_stream_commit(char *buf);
    void process_stream_abort(char *buf);
    void porcess_stream_stop(char *buf);
    void process_truncate(char *buf, int head_len);
    void process_row(relationInfo &info, rowData &row);
    void checkWALData(char *buf, int remaining_head);
    XLogRecPtr received_lsn;
    XLogRecPtr flushed_lsn;
    std::chrono::system_clock::time_point last_feedback_time;

public:
    PostgresServer(int argc, char *const argv[]);
    ~PostgresServer();
    void identifySystem();
    void setSlotandStartReplication(std::string slotName, std::string publicationName);
};

PostgresServer::PostgresServer(int argc, char *const argv[])
{
    received_lsn = 0;
    flushed_lsn = 0;
    auto params = parseParameter(argc, argv);
    conn = std::shared_ptr<PGconn>(PQconnectdb(params.c_str()), PGconnDeleter);
    if (conn == nullptr)
    {
        std::cout << "could not allocate connection object." << std::endl;
        std::exit(-1);
    }
    if (PQstatus(conn.get()) == CONNECTION_OK)
    {
        std::cout << "we have successfully connected to database server \n";
    }
    else
    {
        std::cout << "connection to database server failed \n";
        std::cout << PQerrorMessage(conn.get());
    }
}

PostgresServer::~PostgresServer()
{
}

void PostgresServer::identifySystem()
{
    auto res = std::unique_ptr<PGresult, decltype(PGresultDeleter)>(PQexec(conn.get(), "IDENTIFY_SYSTEM"), PGresultDeleter);
    if (PQresultStatus(res.get()) != PGRES_TUPLES_OK)
    {
        std::cout << "could not identify system \n";
        std::cout << PQerrorMessage(conn.get());
        std::exit(-2);
    }

   /* int nFields = PQnfields(res.get());
    int nTuples = PQntuples(res.get());
    std::cout << " IDENTIFY_SYSTEM get " << nTuples << " row " << std::endl;
    std::cout << " has " << nFields << " row " << std::endl;
    for (int i = 0; i < nFields; i++)
    {
        std::cout << PQgetvalue(res.get(), 0, i) << "\n";
    }*/
}

void PostgresServer::setSlotandStartReplication(std::string slotName, std::string publicationName)
{
    std::string command = "CREATE_REPLICATION_SLOT \"" + slotName + "\" LOGICAL pgoutput (SNAPSHOT 'nothing');";
    auto res = std::unique_ptr<PGresult, decltype(PGresultDeleter)>(PQexec(conn.get(), command.c_str()), PGresultDeleter);
    if (PQresultStatus(res.get()) != PGRES_TUPLES_OK)
    {
        std::cout << "cannot create replication slot. Error is" << PQresultStatus(res.get()) << "\n";
    }
    command = "START_REPLICATION SLOT \"" + slotName + "\" LOGICAL 0/0 (proto_version '3', streaming 'on', publication_names '\"" + publicationName + "\"');";
    res.reset(PQexec(conn.get(), command.c_str()));
    std::cout << "Start receiving data from database server." << std::endl;
    copyBuf = nullptr;
    while (true)
    {
        // std::cout << "\n\n\n开始接收数据\n";
        auto now = std::chrono::system_clock::now();
        checkFeedback();
        int r = PQgetCopyData(conn.get(), &copyBuf, 0); // we use blocked copy here
        if (r == 0)
        {
            std::cout << "no data has been received\n";
            continue;
        }
        if (r == -2)
        {
            std::cout << "replication has been broken. \n";
            std::cout << PQerrorMessage(conn.get()) << std::endl;
            std::exit(-5);
        }
        if (r == -1)
        {
            std::cout << "replication broken. Existing" << std::endl;
            std::cout << PQerrorMessage(conn.get()) << std::endl;
            exit(-6);
        }
        if (copyBuf[0] == 'k')
        {
            process_keepalived_message();

            continue;
        }
        if (copyBuf[0] != 'w')
        {
            std::cout << "received a non-wal log record. Exiting ... \n";
            exit(-7);
        }
        int head_len = 0;
        head_len += 1; // message type 'w'
        head_len += 8; // dataStart
        head_len += 8; // walEnd;
        head_len += 8; // sendTime
        int remaining_head = r - head_len;
        if (r < head_len + 1)
        {
            std::cout << "received data is too short. Exiting ...\n";
            // continue;
            exit(-8);
        }
        auto record_lsn = buf_recev<std::int64_t>(&copyBuf[1]);
        received_lsn = record_lsn == 0 ? received_lsn : record_lsn;
        // std::cout << "received LSN is: " << received_lsn << "\n";
        // std::cout << "data head is " << copyBuf[head_len] << "\n";
        // std::cout << "到这里了\n";
        // std::cout << "开始checkdata数据\n";
        checkWALData(&copyBuf[head_len], remaining_head);
        sendFeedback();
        PQfreemem(copyBuf);
        copyBuf = nullptr;
    }
}

bool PostgresServer::sendFeedback()
{
    auto now = std::chrono::system_clock::now();
    if (received_lsn == 0)
    {
        // std::cout << "没有送\n";
        return true;
    }
    // std::cout << "开始送了\n";
    auto tp_now = convertToPostgresTimestamp(now);
    char replyBuf[1 + 8 + 8 + 8 + 8 + 1];
    int len = 0;
    replyBuf[len] = 'r';
    len += 1;
    buf_send(received_lsn, &replyBuf[len]); // write
    len += 8;
    buf_send(received_lsn, &replyBuf[len]); // flush
    len += 8;
    buf_send(InvalidXLogRecPtr, &replyBuf[len]); // apply
    len += 8;
    buf_send(tp_now, &replyBuf[len]);
    len += 8;
    replyBuf[len] = 0; // don't need reply for now
    len++;
    // std::cout << "LSN is: " << received_lsn << "\n";
    if (PQputCopyData(conn.get(), replyBuf, len) < 0 || PQflush(conn.get()))
    {
        std::cout << "feedback packet could not be sent" << std::endl;
        return false;
    }
    return true;
}

void PostgresServer::checkFeedback()
{
    // std::cout << "开始check feedback\n";
    auto now = std::chrono::system_clock::now();
    if (now - last_feedback_time > std::chrono::seconds(1))
    {
        auto feedBack = sendFeedback();
        if (feedBack == false)
        {
            std::cout << "could not send feedback. Exiting ... \n";
            std::exit(-3);
        }
        last_feedback_time = now;
    }
    // std::cout << "check feedback 结束\n";
}

void PostgresServer::process_keepalived_message()
{
    // std::cout << "开始收到keep alived数据\n";
    int pos = 1; // for 'k'
    auto log_pos = buf_recev<XLogRecPtr> (&copyBuf[pos]);
    received_lsn = std::max(received_lsn, log_pos);
    sendFeedback();
    PQfreemem(copyBuf);
    copyBuf = nullptr;
}

void PostgresServer::checkWALData(char *buf, int head_len)
{
    switch (buf[0])
    {
    case 'R':
        porcess_relation_message(buf);
        break;
    case 'C':
        process_commit_message(buf);
        break;
    case 'I':
        process_insert_message(buf);
        break;
    case 'B':
        process_begin_message(buf);
        break;
    case 'D':
        porcess_delete_message(buf);
        break;
    case 'U':
        process_update_message(buf);
        break;
    case 'A':
        process_stream_abort(buf);
        break;
    case 'c':
        process_stream_commit(buf);
        break;
    case 'S':
        process_stream_start(buf);
        break;
    case 'E':
        porcess_stream_stop(buf);
        break;
    case 'T':
        process_truncate(buf, head_len);
        break;
    default:
        std::cout << "process unknow message, the message is " << buf[0] << "\n";
        break;
    }
}

void PostgresServer::porcess_relation_message(char *buf)
{
    int len = 1; // for 'R'
    struct relationInfo rel_info;
    rel_info.oid = buf_recev<Oid>(&buf[len]);
    len += 4;
    rel_info.nameSpace = std::string(&buf[len]);
    len += static_cast<int>(rel_info.nameSpace.size()) + 1; // c strhing ends with 0, so we need to add 1.
    rel_info.relationName = std::string(&buf[len]);
    len += static_cast<int>(rel_info.relationName.size()) + 1;
    rel_info.replicaIdentity = buf_recev<char>(&buf[len]);
    len += 1; // repilcation identity settings. this is int8.
    rel_info.columnCount = buf_recev<std::int16_t>(&buf[len]);
    len += 2;
    for (int i = 0; i < rel_info.columnCount; i++)
    {
        columnInfo c_info;
        c_info.keyFlag = buf_recev<std::int8_t>(&buf[len]);
        len += 1;
        c_info.columnName = std::string(&buf[len]);
        len += static_cast<int>(c_info.columnName.size()) + 1;
        c_info.columnType = buf_recev<Oid>(&buf[len]);
        len += 4;
        c_info.atttypmod = buf_recev<std::int32_t>(&buf[len]);
        len += 4;
        rel_info.cloumnInfos.push_back(c_info);
    }
    relationMap.insert({rel_info.oid, rel_info});
}

void PostgresServer::process_begin_message(char *buf)
{
    int len = 1;                // for 'B'
    len += sizeof(XLogRecPtr);  // len += 8 for final LSN of the transaction
    len += sizeof(TimestampTz); // len += 8 for commit timestamp
    Xid xid = buf_recev<Xid>(&buf[len]);
    std::cout << "BEGIN: Xid " << xid << "\n";
}

void PostgresServer::process_insert_message(char *buf)
{
    bool is_stream = false;
    Oid relation_id = -1;
    Xid xid = -1;
    int len = 1; // for 'I';
    std::int32_t transaction_id_or_oid = buf_recev<std::int32_t>(&buf[len]);
    len += 4;
    if (buf[len] == 'N')
    {
        relation_id = transaction_id_or_oid;
    }
    else
    {
        is_stream = true;
        xid = transaction_id_or_oid;
        relation_id = buf_recev<Oid>(&buf[len]);
        len += 4;
    }
    auto iter = relationMap.find(relation_id);
    if (iter == relationMap.end())
    {
        std::cout << "received some unknown relation. Exiting ...\n";
        std::cout << "relation id is" << relation_id << "\n\n";
        std::exit(-7);
    }
    auto relation_info = relationMap[relation_id];
    len += 1; // for Byte1('N')
    if (is_stream)
    {
        std::cout << "Streaming, Xid: " << xid << " ";
    }
    std::cout << "table " << relation_info.nameSpace << "."
              << relation_info.relationName << ": INSERT: ";
    auto row = process_tupledata(buf, len);
    for (int i = 0; i < relation_info.columnCount; i++)
    {
        std::cout << relation_info.cloumnInfos[i].columnName << ": "
                  << row.data[i].data << " ";
    }
    std::cout << "\n";
}

rowData PostgresServer::process_tupledata(char *buf, int len)
{
    rowData row;
    row.columnCount = buf_recev<std::int16_t>(&buf[len]);
    len += 2;
    std::string res1;
    for (int i = 0; i < row.columnCount; i++)
    {
        switch (buf[len])
        {
        case 'n':
        {
            len++;
            columnData col_data;
            col_data.type = 'n';
            col_data.len = 0;
            row.data.push_back(col_data);
        }
        break;
        case 'u':
            std::cout << "unchanged TOASTed value" << std::endl;
            len++;
            break;
        case 't':
        {
            len++;
            columnData col_data;
            auto text_len = buf_recev<std::int32_t>(&buf[len]);
            len += 4;
            char *text = new char[text_len + 1];
            std::memcpy(text, &buf[len], text_len);
            text[text_len] = '\0';
            res1 = std::string(text);
            delete[] text;
            len += text_len;
            col_data.type = 't';
            col_data.len = text_len;
            col_data.data = res1;
            row.data.push_back(col_data);
            break;
        }
        default:
            std::cout << "unknow insert type value" << std::endl;
            break;
        }
    }
    row.len = len;
    return row;
}

void PostgresServer::process_commit_message(char *buf)
{
    int len = 1; // for 'C'
    len += 1;    // for unused flag
    auto end_lsn = buf_recev<XLogRecPtr>(&buf[len]);
    received_lsn = end_lsn;
    sendFeedback();
    last_feedback_time = std::chrono::system_clock::now();
    std::cout << "COMMIT\n\n";
}

void PostgresServer::porcess_delete_message(char *buf)
{
    bool is_stream = false;
    Oid relation_id = -1;
    Xid xid = -1;
    int len = 1; // for 'D';
    std::int32_t transaction_id_or_oid = buf_recev<std::int32_t>(&buf[len]);
    len += sizeof(std::int32_t);
    if (buf[len] == 'K' || buf[len] == 'O')
    {
        relation_id = transaction_id_or_oid;
    }
    else
    {
        xid = transaction_id_or_oid;
        relation_id = buf_recev<Oid>(&buf[len]);
        len += sizeof(Oid);
        is_stream = true;
    }
    // Oid relation_id = buf_recev<Oid>(&buf[len]);
    std::string key_type = buf[len] == 'K' ? "INDEX" : "REPLICA IDENTITY";
    len += 1; // for 'K' or 'O'
    auto relation_info = relationMap[relation_id];
    auto row = process_tupledata(buf, len);
    if (is_stream)
    {
        std::cout << "Streaming, Xid: " << xid << " ";
    }
    std::cout << "table " << relation_info.nameSpace << "."
              << relation_info.relationName << ": DELETE: (" << key_type << ") ";

    for (int i = 0; i < relation_info.columnCount; i++)
    {
        if (row.data[i].type == 'n') // NULL value for this column
        {
            continue;
        }
        std::cout << relation_info.cloumnInfos[i].columnName << ": "
                  << row.data[i].data << " ";
    }
    std::cout << "\n";
}

void PostgresServer::process_row(relationInfo &info, rowData &row)
{
    for (int i = 0; i < info.columnCount; i++)
    {
        if (row.data[i].type == 'n') // NULL value for this column
        {
            continue;
        }
        std::cout << info.cloumnInfos[i].columnName << ": "
                  << row.data[i].data << " ";
    }
    // std::cout << "\n";
}

void PostgresServer::process_update_message(char *buf)
{
    int len = 1; // for 'U'
    bool is_stream = false;
    Oid relation_id = -1;
    Xid xid = -1;
    std::int32_t transaction_id_or_oid = buf_recev<std::int32_t>(&buf[len]);
    len += sizeof(std::int32_t);
    // skip for TransactionId as this is not streamed trasaction.
    if (buf[len] == 'K' || buf[len] == 'O' || buf[len] == 'N')
    {
        relation_id = transaction_id_or_oid;
    }
    else
    {
        is_stream = true;
        xid = transaction_id_or_oid;
        relation_id = buf_recev<Oid>(&buf[len]);
        len += sizeof(Oid);
    }
    if (is_stream)
    {
        std::cout << "Streaming, Xid: " << xid << " ";
    }
    auto relation_info = relationMap[relation_id];
    std::cout << "table " << relation_info.nameSpace << "."
              << relation_info.relationName << " UPDATE ";
    switch (buf[len])
    {
    case 'K':
    case 'O':
    {
        std::string key_info = buf[len] == 'K' ? "INDEX: " : "REPLICA IDENTITY: ";
        len += 1; // for 'K' or 'O'
        auto row = process_tupledata(buf, len);
        len = row.len;
        std::cout << "Old " << key_info << ": ";
        process_row(relation_info, row);
        if (buf[len] != 'N')
        {
            std::cout << "no new data\n";
            std::exit(-10);
        }
        len += 1;
        std::cout << "New Row: ";
        row = process_tupledata(buf, len);
        process_row(relation_info, row);
        std::cout << "\n";
        break;
    }
    case 'N':
    {
        len++;
        auto row = process_tupledata(buf, len);
        len = row.len;
        std::cout << "New Row: ";
        process_row(relation_info, row);
        std::cout << "\n";
        break;
    }

    default:
        std::cout << "Unknown data in update\n";
        break;
    }
}

void PostgresServer::process_stream_start(char *buf)
{
    int len = 1; // for 'S'
    Xid xid = buf_recev<Xid>(&buf[len]);
    len += sizeof(Xid);
    std::cout << "Opening a streamed block for transaction " << xid << "\n";
}

void PostgresServer::process_stream_commit(char *buf)
{
    int len = 1; // for 'c'
    Xid xid = buf_recev<Xid>(&buf[len]);
    len += sizeof(Xid);
    len += 1; // for the unused flag.
    received_lsn = buf_recev<XLogRecPtr>(&buf[len]);
    len += sizeof(XLogRecPtr);
    std::cout << "Comitting streamed transaction " << xid << "\n\n";
    sendFeedback();
    last_feedback_time = std::chrono::system_clock::now();
}

void PostgresServer::process_stream_abort(char *buf)
{
    int len = 1; // for 'A'
    Xid xid = buf_recev<Xid>(&buf[len]);
    len += sizeof(Xid);
    std::cout << "Aborting streamed transaction " << xid << "\n";
}

void PostgresServer::porcess_stream_stop(char *buf)
{
    int len = 1; // for 'E'
    std::cout << "Stream Stop\n";
}

void PostgresServer::process_truncate(char *buf, int head_len)
{
    bool is_stream = false;
    Xid xid = -1;
    Oid relation_id = -1;
    std::int32_t relation_num = -1;
    std::vector<Oid> oids;
    int len = 1; // for 'T'
    std::int32_t xid_or_num_relations = buf_recev<std::int32_t>(&buf[len]);
    len += sizeof(std::int32_t);
    std::int32_t possible_relation_num = buf_recev<std::int32_t>(&buf[len]);
    len += sizeof(std::int32_t);
    int remaining = head_len - len;
    if ((sizeof(std::int8_t) + sizeof(Oid) * possible_relation_num) == remaining)
    {
        is_stream = true;
        xid = xid_or_num_relations;
        relation_num = possible_relation_num;
    }
    else
    {
        relation_num = xid_or_num_relations;
        len -= sizeof(std::int32_t); // there is no transaction id, so we need to go back to find the option bits.
    }
    std::int8_t flag = buf_recev<std::int8_t>(&buf[len]);
    len += sizeof(std::int8_t);
    std::string flag_bits = " ";
    if (flag == 1)
    {
        flag_bits = "CASCADE ";
    }
    if (flag == 2)
    {
        flag_bits = "RESTART IDENTITY ";
    }

    for (int i = 0; i < relation_num; i++)
    {
        Oid table = buf_recev<Oid>(&buf[len]);
        len += sizeof(Oid);
        oids.push_back(table);
    }

    if (is_stream)
    {
        std::cout << "Streaming, Xid: " << xid << " ";
    }
    std::cout << "TRUNCATE " << flag_bits;
    for (auto rel : oids)
    {
        auto iter = relationMap.find(rel);
        if (iter == relationMap.end())
        {
            std::cout << "cannot find relation in truncate, oid is " << rel << "\n";
            return;
        }
        auto table_info = relationMap[rel];
        std::cout << table_info.nameSpace << "." << table_info.relationName << " ";
    }
    std::cout << "\n";
}

#endif