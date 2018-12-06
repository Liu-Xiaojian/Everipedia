/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

using namespace eosio;
using std::string;

class [[eosio::contract("everipediaiq")]] everipediaiq : public contract {
  using contract::contract;

  public:

     [[eosio::action]]
     void burn( name from,
                   asset        quantity,
                   string       memo );

     [[eosio::action]]
     void create( name issuer,
                  asset        maximum_supply);

     [[eosio::action]]
     void issue( name to, asset quantity, string memo );

     [[eosio::action]]
     void transfer( name from,
                    name to,
                    asset        quantity,
                    string       memo );

     [[eosio::action]]
     void paytxfee( name from,
                   asset        quantity,
                   string       memo );

     [[eosio::action]]
     void brainmeiq( name staker,
                   int64_t amount );


     inline asset get_supply( symbol_code sym )const;

     inline asset get_balance( name owner, symbol_code sym )const;

  private:
     const name TOKEN_CONTRACT_ACCTNAME = name("everipediaiq");
     const name ARTICLE_CONTRACT_ACCTNAME = name("eparticlectr");
     const name FEE_CONTRACT_ACCTNAME = name("epiqtokenfee");
     const name GOVERNANCE_CONTRACT_ACCTNAME = name("epgovernance");
     const name SAFESEND_CONTRACT_ACCTNAME = name("iqsafesendiq");
     eosio::symbol IQSYMBOL = symbol(symbol_code("IQ"), 3);
     const int64_t IQ_PRECISION_MULTIPLIER = 1000;

     struct [[eosio::table]] account {
        asset    balance;

        uint64_t primary_key()const { return balance.symbol.code().raw(); }
     };

     struct [[eosio::table]] currency_stats {
        asset          supply;
        asset          max_supply;
        name   issuer;

        uint64_t primary_key()const { return supply.symbol.code().raw(); }
     };

     typedef eosio::multi_index<name("accounts"), account> accounts;
     typedef eosio::multi_index<name("stat"), currency_stats> stats;

     void sub_balance( name owner, asset value );
     void add_balance( name owner, asset value, name ram_payer );

  public:
     struct transfer_args {
        name   from;
        name   to;
        asset  quantity;
        string memo;
     };
};

asset everipediaiq::get_supply( symbol_code sym )const
{
  stats statstable( _self, sym.raw() );
  const auto& st = statstable.get( sym.raw() );
  return st.supply;
}

asset everipediaiq::get_balance( name owner, symbol_code sym )const
{
  accounts accountstable( _self, owner.value );
  const auto& ac = accountstable.get( sym.raw() );
  return ac.balance;
}
