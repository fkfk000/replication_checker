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


## docker run

docker run --name pg-replica-checker -e PubName=pub -e SlotName=sub -it dog830228/replica-pg:0.2 /replication_checker/build/replication_checker user postgres replication database host localhost dbname postgres password test.123
