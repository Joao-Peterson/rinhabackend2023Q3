#include "../src/db.h"

// pessoas migrations
db_error_code_t pessoa_migrate(db_t *db, bool dropTables){
	char *query;
	if(dropTables){
		query = "drop table pessoas;"
		"create table pessoas("
			"id uuid primary key,"
			"apelido varchar(32) unique not null,"
			"nome varchar(100),"
			"nascimento varchar(8) not null,"
			"stack varchar(32)[]"
        ");";
	}else{
		query = "create table pessoas("
			"id uuid primary key,"
			"apelido varchar(32) unique not null,"
			"nome varchar(100),"
			"nascimento varchar(8) not null,"
			"stack varchar(32)[]"
        ");";
	}

	return db_exec(db, query, 0);
}

// insert model into db
db_error_code_t pessoas_insert(db_t *db, char *nome, char *apelido, char *nascimento, char *stack){
	char *query = "insert into pessoas "
	 	"(id, apelido, nome, nascimento, stack) "
	 	"values("
			"gen_random_uuid(),"
			"$1,"
			"$2,"
			"$3,"
			"$4"
		") "
		"returning id";

	return db_exec(db, query, 4, 
		db_param_new(db_type_string, apelido, 0),
		db_param_new(db_type_string, nome, 0),
		db_param_new(db_type_string, nascimento, 0),
		db_param_new(db_type_string, stack, 0)
	);
}

// search
db_error_code_t pessoas_select_search(db_t *db, char *searchParam, unsigned int limit){
	char *query = "select * "
		"from pessoas "
		"where ("
			"apelido ilike '%' || $1 || '%' or "
			"nome ilike '%' || $1 || '%' or "
			"$1 = any(stack)"
		") limit $2;";


	char limitstr[10];
	snprintf(limitstr, 9, "%d", limit);

	return db_exec(db, query, 2, 
		db_param_new(db_type_string, searchParam, 0),
		db_param_new(db_type_string, limitstr, 0)
	);
}

// search
db_error_code_t pessoas_select_uuid(db_t *db, char *uuid){
	char *query = "select * "
		"from pessoas "
		"where id = $1";

	return db_exec(db, query, 1, 
		db_param_new(db_type_string, uuid, 0)
	);
}

// count
db_error_code_t pessoas_count(db_t *db){
	char *query = "select count(*) from pessoas;";

	return db_exec(db, query, 0);
}