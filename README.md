# replication_checker
This would act like a Postgres replica server to consume the logical changes in server. This is for sharing in Orcas.

## Compile
This prject uses cmake to compile. To minimize the effort we need to spend during the configuration, I have decided not to include the require part. Instead, we only need
- download pre-compiled Postgres server files.
- download pre-compiled openssl files.
and then modify the path.

Then, 
```
mkdir bld
cd bld
cmake ..
cmake --build .
```
would compile the progrem

## Run this program
.\Debug\replication_checker.exe user username replication database host host.postgres.database.azure.com dbname test1 password LongPassword

Then, the application will connect to database server and receving the changes. When data chnages happen in database server, the changes will be displayed by this application.


![image](https://github.com/fkfk000/replication_checker/assets/14956155/a5d65fb9-75ea-45a7-b606-a44414afe6f4)

For exmaple, for the partioned table "tb", if we insert into some values, replication_checker will display the logical part for this table:

![image](https://github.com/fkfk000/replication_checker/assets/14956155/59cf1da8-5eb6-44c8-b6f8-4bd33f40c422)

And we could see the lag from the database side (becasue we do not replay the wal files, the replay lag would just increase):
![image](https://github.com/fkfk000/replication_checker/assets/14956155/09b94fbe-c181-46c0-8dc3-6fd0676281b0)



## How replication_checker works
In Postgres server, replica would connect to database server just like normal connections expcet add "replication=database" in connection string. In replication_checker, the input parameters would be pasred as "user=username replication=database host=host.postgres.database.azure.com dbname=test1 password=LongPassword".

Then, replication_checker will connect to database use the parsed connection string and looking for publication "pub" and create a subscription "sub". (this is currently hard coded, perhaps I will change in the feature)

Then, primary server will send data changes to replication_checker. 

This programs supports logical replication protocols like "insert/update/delete" both in normal logical replication mode or streaming mode. For example

![image](https://github.com/fkfk000/replication_checker/assets/14956155/1764d4a4-0710-4008-90d6-80adefb3cbff)

Then, in replication_checker we could have:


![image](https://github.com/fkfk000/replication_checker/assets/14956155/96ef0414-dff8-4ab4-b28f-643c4bf63b33)


## docker run

```
docker run -e PubName=pub -e SlotName=sub -it dog830228/replica-pg:0.5  user postgres replication database host localhost dbname postgres password test.123
```













