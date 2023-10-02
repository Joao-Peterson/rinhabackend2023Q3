-- immutable function to be used on the 'search' generated column, just a wrapper around 'array to string' to be used on the 'stack' string array
create or replace function immutable_array_to_string(text[], text) 
    returns text as $$ select array_to_string($1, $2); $$ 
language sql immutable;

-- db create
create table pessoas(
	id uuid primary key,
	apelido varchar(32) unique not null,
	nome varchar(100),
	nascimento varchar(10) not null,
	stack varchar(32)[],

	-- search columns, concatenate nome apelido and stack values to index and search later
	search text generated always as ( lower( nome || apelido || immutable_array_to_string(stack, ' ') ) ) stored
);

-- https://about.gitlab.com/blog/2016/03/18/fast-search-using-postgresql-trigram-indexes/
-- loads postgres trigram
create extension pg_trgm;
-- creates index for search using trigram gist
create index concurrently if not exists idx_pessoas_search on pessoas using gist (search gist_trgm_ops(siglen=64));