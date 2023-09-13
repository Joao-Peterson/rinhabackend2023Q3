create table pessoas(
	id uuid primary key,
	apelido varchar(32) unique not null,
	nome varchar(100),
	nascimento varchar(10) not null,
	stack varchar(32)[]
);