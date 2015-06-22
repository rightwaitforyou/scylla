/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copyright 2015 Cloudius Systems
 *
 * Modified by Cloudius Systems
 */

#include "cql3/statements/create_table_statement.hh"

#include "schema_builder.hh"

namespace cql3 {

namespace statements {

create_table_statement::create_table_statement(::shared_ptr<cf_name> name,
                                               ::shared_ptr<cf_prop_defs> properties,
                                               bool if_not_exists,
                                               column_set_type static_columns)
    : schema_altering_statement{name}
    , _static_columns{static_columns}
    , _properties{properties}
    , _if_not_exists{if_not_exists}
{
#if 0
    try
    {
        if (!this.properties.hasProperty(CFPropDefs.KW_COMPRESSION) && CFMetaData.DEFAULT_COMPRESSOR != null)
            this.properties.addProperty(CFPropDefs.KW_COMPRESSION,
                                        new HashMap<String, String>()
                                        {{
                                            put(CompressionParameters.SSTABLE_COMPRESSION, CFMetaData.DEFAULT_COMPRESSOR);
                                        }});
    }
    catch (SyntaxException e)
    {
        throw new AssertionError(e);
    }
#endif
}

void create_table_statement::check_access(const service::client_state& state) {
    warn(unimplemented::cause::PERMISSIONS);
#if 0
    state.hasKeyspaceAccess(keyspace(), Permission.CREATE);
#endif
}

void create_table_statement::validate(distributed<service::storage_proxy>&, const service::client_state& state) {
    // validated in announceMigration()
}

// Column definitions
std::vector<column_definition> create_table_statement::get_columns()
{
    std::vector<column_definition> column_defs;
    for (auto&& col : _columns) {
        column_kind kind = column_kind::regular_column;
        if (_static_columns.count(col.first)) {
            kind = column_kind::static_column;
        }
        column_defs.emplace_back(col.first->name(), col.second, kind);
    }
    return column_defs;
}

future<bool> create_table_statement::announce_migration(distributed<service::storage_proxy>& proxy, bool is_local_only) {
    return service::migration_manager::announce_new_column_family(proxy, get_cf_meta_data(), is_local_only).then_wrapped([this] (auto&& f) {
        try {
            f.get();
            return true;
        } catch (const exceptions::already_exists_exception& e) {
            if (_if_not_exists) {
                return false;
            }
            throw e;
        }
    });
}

shared_ptr<transport::event::schema_change> create_table_statement::change_event() {
    return make_shared<transport::event::schema_change>(transport::event::schema_change::change_type::CREATED, transport::event::schema_change::target_type::TABLE, keyspace(), column_family());
}

/**
 * Returns a CFMetaData instance based on the parameters parsed from this
 * <code>CREATE</code> statement, or defaults where applicable.
 *
 * @return a CFMetaData instance corresponding to the values parsed from this statement
 * @throws InvalidRequestException on failure to validate parsed parameters
 */
schema_ptr create_table_statement::get_cf_meta_data() {
    schema_builder builder{keyspace(), column_family()};
    apply_properties_to(builder);
    return builder.build();
}

void create_table_statement::apply_properties_to(schema_builder& builder) {
    auto&& columns = get_columns();
    for (auto&& column : columns) {
        builder.with_column(column);
    }
#if 0
    cfmd.defaultValidator(defaultValidator)
        .keyValidator(keyValidator)
        .addAllColumnDefinitions(getColumns(cfmd))
        .isDense(isDense);
#endif

    add_column_metadata_from_aliases(builder, _key_aliases, _partition_key_types, column_kind::partition_key);
    add_column_metadata_from_aliases(builder, _column_aliases, _clustering_key_types, column_kind::clustering_key);
#if 0
    if (valueAlias != null)
        addColumnMetadataFromAliases(cfmd, Collections.singletonList(valueAlias), defaultValidator, ColumnDefinition.Kind.COMPACT_VALUE);
#endif

    _properties->apply_to_builder(builder);
}

void create_table_statement::add_column_metadata_from_aliases(schema_builder& builder, std::vector<bytes> aliases, const std::vector<data_type>& types, column_kind kind)
{
    assert(aliases.size() == types.size());
    for (size_t i = 0; i < aliases.size(); i++) {
        if (!aliases[i].empty()) {
            builder.with_column(aliases[i], types[i], kind);
        }
    }
}

create_table_statement::raw_statement::raw_statement(::shared_ptr<cf_name> name, bool if_not_exists)
    : cf_statement{std::move(name)}
    , _if_not_exists{if_not_exists}
{ }

::shared_ptr<parsed_statement::prepared> create_table_statement::raw_statement::prepare(database& db) {
#if 0
    // Column family name
    if (!columnFamily().matches("\\w+"))
        throw new InvalidRequestException(String.format("\"%s\" is not a valid table name (must be alphanumeric character only: [0-9A-Za-z]+)", columnFamily()));
    if (columnFamily().length() > Schema.NAME_LENGTH)
        throw new InvalidRequestException(String.format("Table names shouldn't be more than %s characters long (got \"%s\")", Schema.NAME_LENGTH, columnFamily()));

    for (Multiset.Entry<ColumnIdentifier> entry : definedNames.entrySet())
        if (entry.getCount() > 1)
            throw new InvalidRequestException(String.format("Multiple definition of identifier %s", entry.getElement()));
#endif

    properties->validate();

    auto stmt = ::make_shared<create_table_statement>(_cf_name, properties, _if_not_exists, _static_columns);

    std::experimental::optional<std::map<bytes, data_type>> defined_multi_cell_collections;
    for (auto&& entry : _definitions) {
        ::shared_ptr<column_identifier> id = entry.first;
        ::shared_ptr<cql3_type> pt = entry.second->prepare(db, keyspace());
        if (pt->is_collection() && pt->get_type()->is_multi_cell()) {
            if (!defined_multi_cell_collections) {
                defined_multi_cell_collections = std::map<bytes, data_type>{};
            }
            defined_multi_cell_collections->emplace(id->name(), pt->get_type());
        }
        stmt->_columns.emplace(id, pt->get_type()); // we'll remove what is not a column below
    }
    if (_key_aliases.empty()) {
        throw exceptions::invalid_request_exception("No PRIMARY KEY specifed (exactly one required)");
    } else if (_key_aliases.size() > 1) {
        throw exceptions::invalid_request_exception("Multiple PRIMARY KEYs specifed (exactly one required)");
    }

    auto& key_aliases = _key_aliases[0];
    std::vector<data_type> key_types;
    for (auto&& alias : key_aliases) {
        stmt->_key_aliases.emplace_back(alias->name());
        auto t = get_type_and_remove(stmt->_columns, alias);
        if (t->is_counter()) {
            throw exceptions::invalid_request_exception(sprint("counter type is not supported for PRIMARY KEY part %s", alias->text()));
        }
        if (_static_columns.count(alias) > 0) {
            throw exceptions::invalid_request_exception(sprint("Static column %s cannot be part of the PRIMARY KEY", alias->text()));
        }
        key_types.emplace_back(t);
    }
    stmt->_partition_key_types = key_types;

#if 0
    // Dense means that no part of the comparator stores a CQL column name. This means
    // COMPACT STORAGE with at least one columnAliases (otherwise it's a thrift "static" CF).
    stmt.isDense = useCompactStorage && !columnAliases.isEmpty();
#endif

    // Handle column aliases
    if (_column_aliases.empty()) {
        if (_use_compact_storage) {
            // There should remain some column definition since it is a non-composite "static" CF
            if (stmt->_columns.empty()) {
                throw exceptions::invalid_request_exception("No definition found that is not part of the PRIMARY KEY");
            }
            if (defined_multi_cell_collections) {
                throw exceptions::invalid_request_exception("Non-frozen collection types are not supported with COMPACT STORAGE");
            }
            stmt->_clustering_key_types.emplace_back(utf8_type);
        } else {
            stmt->_clustering_key_types = std::vector<data_type>{};
        }
    } else {
        // If we use compact storage and have only one alias, it is a
        // standard "dynamic" CF, otherwise it's a composite
        if (_use_compact_storage && _column_aliases.size() == 1) {
            if (defined_multi_cell_collections) {
                throw exceptions::invalid_request_exception("Collection types are not supported with COMPACT STORAGE");
            }
            auto alias = _column_aliases[0];
            if (_static_columns.count(alias) > 0) {
                throw exceptions::invalid_request_exception(sprint("Static column %s cannot be part of the PRIMARY KEY", alias->text()));
            }
            stmt->_column_aliases.emplace_back(alias->name());
            auto at = get_type_and_remove(stmt->_columns, alias);
            if (at->is_counter()) {
                throw exceptions::invalid_request_exception(sprint("counter type is not supported for PRIMARY KEY part %s", stmt->_column_aliases[0]));
            }
            stmt->_clustering_key_types.emplace_back(at);
        } else {
            std::vector<data_type> types;
            for (auto&& t : _column_aliases) {
                stmt->_column_aliases.emplace_back(t->name());
                auto type = get_type_and_remove(stmt->_columns, t);
                if (type->is_counter()) {
                    throw exceptions::invalid_request_exception(sprint("counter type is not supported for PRIMARY KEY part %s", t->text()));
                }
                if (_static_columns.count(t) > 0) {
                    throw exceptions::invalid_request_exception(sprint("Static column %s cannot be part of the PRIMARY KEY", t->text()));
                }
                types.emplace_back(type);
            }

            if (_use_compact_storage) {
                if (defined_multi_cell_collections) {
                    throw exceptions::invalid_request_exception("Collection types are not supported with COMPACT STORAGE");
                }
                stmt->_clustering_key_types = types;
            } else {
                stmt->_clustering_key_types = types;
            }
        }
    }

#if 0
    if (!staticColumns.isEmpty())
    {
        // Only CQL3 tables can have static columns
        if (useCompactStorage)
            throw new InvalidRequestException("Static columns are not supported in COMPACT STORAGE tables");
        // Static columns only make sense if we have at least one clustering column. Otherwise everything is static anyway
        if (columnAliases.isEmpty())
            throw new InvalidRequestException("Static columns are only useful (and thus allowed) if the table has at least one clustering column");
    }

    if (useCompactStorage && !stmt.columnAliases.isEmpty())
    {
        if (stmt.columns.isEmpty())
        {
            // The only value we'll insert will be the empty one, so the default validator don't matter
            stmt.defaultValidator = BytesType.instance;
            // We need to distinguish between
            //   * I'm upgrading from thrift so the valueAlias is null
            //   * I've defined my table with only a PK (and the column value will be empty)
            // So, we use an empty valueAlias (rather than null) for the second case
            stmt.valueAlias = ByteBufferUtil.EMPTY_BYTE_BUFFER;
        }
        else
        {
            if (stmt.columns.size() > 1)
                throw new InvalidRequestException(String.format("COMPACT STORAGE with composite PRIMARY KEY allows no more than one column not part of the PRIMARY KEY (got: %s)", StringUtils.join(stmt.columns.keySet(), ", ")));

            Map.Entry<ColumnIdentifier, AbstractType> lastEntry = stmt.columns.entrySet().iterator().next();
            stmt.defaultValidator = lastEntry.getValue();
            stmt.valueAlias = lastEntry.getKey().bytes;
            stmt.columns.remove(lastEntry.getKey());
        }
    }
    else
    {
        // For compact, we are in the "static" case, so we need at least one column defined. For non-compact however, having
        // just the PK is fine since we have CQL3 row marker.
        if (useCompactStorage && stmt.columns.isEmpty())
            throw new InvalidRequestException("COMPACT STORAGE with non-composite PRIMARY KEY require one column not part of the PRIMARY KEY, none given");

        // There is no way to insert/access a column that is not defined for non-compact storage, so
        // the actual validator don't matter much (except that we want to recognize counter CF as limitation apply to them).
        stmt.defaultValidator = !stmt.columns.isEmpty() && (stmt.columns.values().iterator().next() instanceof CounterColumnType)
            ? CounterColumnType.instance
            : BytesType.instance;
    }


    // If we give a clustering order, we must explicitly do so for all aliases and in the order of the PK
    if (!definedOrdering.isEmpty())
    {
        if (definedOrdering.size() > columnAliases.size())
            throw new InvalidRequestException("Only clustering key columns can be defined in CLUSTERING ORDER directive");

        int i = 0;
        for (ColumnIdentifier id : definedOrdering.keySet())
        {
            ColumnIdentifier c = columnAliases.get(i);
            if (!id.equals(c))
            {
                if (definedOrdering.containsKey(c))
                    throw new InvalidRequestException(String.format("The order of columns in the CLUSTERING ORDER directive must be the one of the clustering key (%s must appear before %s)", c, id));
                else
                    throw new InvalidRequestException(String.format("Missing CLUSTERING ORDER for column %s", c));
            }
            ++i;
        }
    }
#endif

    return ::make_shared<parsed_statement::prepared>(stmt);
}

data_type create_table_statement::raw_statement::get_type_and_remove(column_map_type& columns, ::shared_ptr<column_identifier> t)
{
    auto it = columns.find(t);
    if (it == columns.end()) {
        throw exceptions::invalid_request_exception(sprint("Unknown definition %s referenced in PRIMARY KEY", t->text()));
    }
    auto type = it->second;
    if (type->is_collection() && type->is_multi_cell()) {
        throw exceptions::invalid_request_exception(sprint("Invalid collection type for PRIMARY KEY component %s", t->text()));
    }
    columns.erase(t);
#if 0
    // FIXME: reversed types are not supported
    Boolean isReversed = definedOrdering.get(t);
    return isReversed != null && isReversed ? ReversedType.getInstance(type) : type;
#endif
    return type;
}

void create_table_statement::raw_statement::add_definition(::shared_ptr<column_identifier> def, ::shared_ptr<cql3_type::raw> type, bool is_static) {
    _defined_names.emplace(def);
    _definitions.emplace(def, type);
    if (is_static) {
        _static_columns.emplace(def);
    }
}

void create_table_statement::raw_statement::add_key_aliases(const std::vector<::shared_ptr<column_identifier>> aliases) {
    _key_aliases.emplace_back(aliases);
}

void create_table_statement::raw_statement::add_column_alias(::shared_ptr<column_identifier> alias) {
    _column_aliases.emplace_back(alias);
}

void create_table_statement::raw_statement::set_ordering(::shared_ptr<column_identifier> alias, bool reversed) {
    defined_ordering.emplace_back(alias, reversed);
}

void create_table_statement::raw_statement::set_compact_storage() {
    _use_compact_storage = true;
}

}

}