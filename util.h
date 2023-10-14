#ifndef UTIL_H
#define UTIL_H

#include <chrono>
#include <libpq-fe.h>
#include <pgtypes_date.h>
#include <memory>
#include <bit>
#include <vector>
#include <iostream>
#include <cstring>

using XLogRecPtr = std::uint64_t;
using Xid = std::int32_t;
#define InvalidXLogRecPtr 0

#define UNIX_EPOCH_JDATE 2440588     /* == date2j(1970, 1, 1) */
#define POSTGRES_EPOCH_JDATE 2451545 /* == date2j(2000, 1, 1) */
#define SECS_PER_DAY 86400

XLogRecPtr received_lsn = 0;
//XLogRecPtr end_pos = 0;

auto postgres_diff = std::chrono::seconds((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY);
auto postgres_diff_micro = std::chrono::duration_cast<std::chrono::microseconds>(postgres_diff);

TimestampTz convertToPostgresTimestamp(std::chrono::system_clock::time_point tp)
{
    auto tp_micro_count = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
    return tp_micro_count - postgres_diff_micro.count();
}

std::chrono::system_clock::time_point convertFromPostgresTimestamp(TimestampTz tz)
{
    tz += postgres_diff_micro.count();
    std::chrono::system_clock::time_point tp;
    auto time = std::chrono::microseconds(tz);
    tp = std::chrono::time_point<std::chrono::system_clock>(time);
    return tp;
}

auto PGconnDeleter = [](PGconn *conn)
{
    if (conn != nullptr)
    {
        PQfinish(conn);
    }
};

auto PGresultDeleter = [](PGresult *res)
{
    if (res != nullptr)
    {
        PQclear(res);
    }
};

auto copyBuffDeleter = [](char *buf)
{
    if (buf != nullptr)
    {
        PQfreemem(buf);
        buf = nullptr;
    }
};

template <typename T>
T buf_recev(char *buf)
{
    T val;
    std::memcpy(&val, buf, sizeof(T));
    if (std::endian::native == std::endian::little)
    {
        return std::byteswap(val);
    }
    else
    {
        return val;
    }
}

template <typename T>
void buf_send(T i, char *buf)
{
    auto val = i;
    if (std::endian::native == std::endian::little)
    {
        val = std::byteswap(i);
    }
    std::memcpy(buf, &val, sizeof(val));
}

bool sendFeedback2(std::shared_ptr<PGconn> conn, std::chrono::system_clock::time_point now)
{
    if (received_lsn == 0)
    {
        return true;
    }
    auto tp_now = convertToPostgresTimestamp(now);
    char replyBuf[1 + 8 + 8 + 8 + 8 + 1];
    int len = 0;
    replyBuf[len] = 'r';
    len += 1;
    buf_send(received_lsn, &replyBuf[len]); // write
    len += 8;
    buf_send(received_lsn, &replyBuf[len]); // flush
    len += 8;
    buf_send(received_lsn, &replyBuf[len]); // apply
    len += 8;
    buf_send(tp_now, &replyBuf[len]);
    len += 8;
    replyBuf[len] = 0; // 暂时不需要reply
    len++;
    if (PQputCopyData(conn.get(), replyBuf, len) < 0 || PQflush(conn.get()))
    {
        std::cout << "could not send feedback package" << std::endl;
        return false;
    }
    return true;
}

// this would parse the parameters and make a
// host=localhost port=5432 dbname=mydb connect_timeout=10 like string
std::string parseParameter(int argc, char *const argv[])
{
    std::string info;
    for (int i = 1; i < argc; i++)
    {
        info += std::string(argv[i]);
        if ((i + 2) % 2 == 1) // make the initial value could count
        {
            info += "=";
        }
        else 
        {
            info += " ";
        }
    }
    return info;
}

#endif