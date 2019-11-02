// # 2018 Travis Moore, Kedar Iyer, Sam Kazemian
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMmdhhydNMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMNdy    hMMMMMMNdhhmMMMddMMMMMMMMMMMMM
// # MMMMMMMMMMMmh      hMMMMMMh     yMMM  hNMMMMMMMMMM
// # MMMMMMMMMNy       yMMMMMMh       MMMh   hNMMMMMMMM
// # MMMMMMMMd         dMMMMMM       hMMMh     NMMMMMMM
// # MMMMMMMd          dMMMMMN      hMMMm       mMMMMMM
// # MMMMMMm           yMMMMMM      hmmh         NMMMMM
// # MMMMMMy            hMMMMMm                  hMMMMM
// # MMMMMN             hNMMMMMmy                 MMMMM
// # MMMMMm          ymMMMMMMMMmd                 MMMMM
// # MMMMMm         dMMMMMMMMd                    MMMMM
// # MMMMMMy       mMMMMMMMm                     hMMMMM
// # MMMMMMm      dMMMMMMMm                      NMMMMM
// # MMMMMMMd     NMMMMMMM                      mMMMMMM
// # MMMMMMMMd    NMMMMMMN                     mMMMMMMM
// # MMMMMMMMMNy  mMMMMMMM                   hNMMMMMMMM
// # MMMMMMMMMMMmyyNMMMMMMm         hmh    hNMMMMMMMMMM
// # MMMMMMMMMMMMMNmNMMMMMMMNmdddmNNd   ydNMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMNdhyhdmMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMNNMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM

#include "eparticlectr.hpp"

// Increase the boost amount for an article
// Users have to trigger this action through the everipediaiq::epartboost action
[[eosio::action]]
void eparticlectr::boostinvest( name booster, uint64_t amount, std::string slug, std::string lang_code ){
    require_auth( _self );

    // Initialize the variable
    int64_t total_boost = 0;

    // Update the booststbl table, or create it if an existing boost isn't already there
    booststbl articleboosts( _self, _self.value );
    auto boost_it = articleboosts.find( sha256_slug_lang_name(slug, lang_code, booster) );

    // Create the new boost entry if it doesn't exist
    if (boost_it == articleboosts.end()) {
        total_boost = amount;
        articleboosts.emplace( _self, [&]( auto& b ) {
            b.id = articleboosts.available_primary_key();
            b.slug = slug;
            b.lang_code = lang_code;
            b.booster = booster;
            b.amount = amount;
            b.timestamp = eosio::current_time_point().sec_since_epoch();
        });
    }
    else {
        total_boost = boost_it->amount + amount;
        articleboosts.modify( boost_it, _self, [&]( auto& b ) {
            b.amount = total_boost;
            b.timestamp = eosio::current_time_point().sec_since_epoch();
        });
    }

}

// Transfer an article's boost amount (fully or partially) to another account and wiki
[[eosio::action]]
void eparticlectr::boosttxfr( 
    name booster, 
    name target, 
    uint64_t amount, 
    std::string src_slug,
    std::string src_lang_code,
    std::string dest_slug, 
    std::string dest_lang_code 
){
    require_auth( booster );
    require_recipient( target );

    // Initialize the source boost table
    booststbl articleboosts( _self, _self.value );
    auto boost_idx = articleboosts.get_index<name("sluglangname")>();
    auto boost_it_src = boost_idx.find( sha256_slug_lang_name(src_slug, src_lang_code, booster) );

    // Checks
    eosio::check(boost_it_src != boost_idx.end(), "Source boost does not exist");
    eosio::check(boost_it_src->timestamp - eosio::current_time_point().sec_since_epoch() >= BOOST_TRANSFER_WAITING_PERIOD, "You must wait until the boost is eligible to be transferred!");
    eosio::check(boost_it_src->amount >= amount, "Not enough boost to transfer!");
    eosio::check(boost_it_src->booster == booster, "Only owner can transfer boost");

    // Initialize the target boost table
    auto boost_it_tgt = boost_idx.find( sha256_slug_lang_name(dest_slug, dest_lang_code, target) );

    // Entire boost is transferred
    if (boost_it_src->amount == amount) {
        // Erase the entry for the source boost
        boost_idx.erase( boost_it_src );
    }
    // Boost is partially transferred
    else {
        // Subtract the source's specified boost amount
        boost_idx.modify( boost_it_src, _self, [&]( auto& b ) {
            b.amount = boost_it_src->amount - amount;
            b.timestamp = eosio::current_time_point().sec_since_epoch();
        });
    }
        
    // Add the source's boost to the target
    // Create the new target boost entry if it doesn't exist
    if (boost_it_tgt == boost_idx.end()) {
        // Set the target's boost iterator to the new entry
        articleboosts.emplace( _self, [&]( auto& b ) {
            b.id = articleboosts.available_primary_key();
            b.slug = dest_slug;
            b.lang_code = dest_lang_code;
            b.booster = target;
            b.amount = amount;
            b.timestamp = eosio::current_time_point().sec_since_epoch();
        });
    }
    // Or add to the target's existing boost
    else{
        boost_idx.modify( boost_it_tgt, _self, [&]( auto& b ) {
            b.amount = boost_it_tgt->amount + amount;
            b.timestamp = eosio::current_time_point().sec_since_epoch();
        });
    }
}

// Redeem IQ, with a specific stake specified
[[eosio::action]]
void eparticlectr::brainclmid( uint64_t stakeid ) {

    // Get the stakes
    staketbl staketable(_self, _self.value);
    auto stake_it = staketable.find(stakeid);
    eosio::check( stake_it != staketable.end(), "Stake does not exist in database");

    // See if the stake is complete
    eosio::check( eosio::current_time_point().sec_since_epoch() > stake_it->completion_time, "Staking period not over yet");

    // Transfer back the IQ
    asset iqAssetPack = asset(int64_t(stake_it->amount * IQ_PRECISION_MULTIPLIER), IQSYMBOL);
    std::string memo = std::string("return stake #") + std::to_string(stake_it->id);
    action(
        permission_level{ _self, name("active") },
        TOKEN_CONTRACT, name("transfer"),
        std::make_tuple(_self, stake_it->user, iqAssetPack, memo)
    ).send();

    // Delete the stake.
    // Note that the erase() function increments the iterator, then gives it back after the erase is done
    stake_it = staketable.erase(stake_it);
}


// Place a vote using the IPFS hash
// Users have to trigger this action through the everipediaiq::epartvote action
[[eosio::action]]
void eparticlectr::vote( name voter, uint64_t proposal_id, bool approve, uint64_t amount, std::string comment, std::string memo ) {
    votestbl votetbl( _self, proposal_id );

    // validate inputs
    eosio::check(comment.size() < MAX_COMMENT_SIZE, "Comment must be less than 256 bytes");
    eosio::check(memo.size() < MAX_MEMO_SIZE, "Memo must be less than 32 bytes");

    // Check if proposal exists
    propstbl proptable( _self, _self.value );
    auto prop_it = proptable.find( proposal_id );
    eosio::check( prop_it != proptable.end(), "Proposal not found" );

    // Verify voting is still in progress
    eosio::check( eosio::current_time_point().sec_since_epoch() < prop_it->endtime, "Voting period is over");
    eosio::check( !prop_it->finalized, "Proposal is finalized");

    // Initialize the boost table
    booststbl articleboosts( _self, _self.value );
    auto boost_idx = articleboosts.get_index<name("bywikiid")>();
    auto boost_it = boost_idx.find( prop_it->wiki_id );

    // Loop through all of the boosts for a given wiki_id and find the one that belongs to the booster, if there is one
    while(boost_it != boost_idx.end() && boost_it->booster != voter) {
        boost_it++;
    }

    // Default boost is 1 (i.e. no boost)
    float boost_multiplier = 1.0;
    if (boost_it != boost_idx.end() && boost_it->booster == voter){
        boost_multiplier = eparticlectr::get_boost_multiplier(boost_it->amount);
        std::string multiplier_string = std::to_string(boost_multiplier) + std::string("x");
        std::string debug_msg = std::string("Multiplying vote by ") + multiplier_string + std::string(" due to ") + std::to_string(boost_it->amount) + std::string("IQ boost present");
        eosio::print(debug_msg);
    } 

    // Verify balances are available
    stats statstbl( _self, voter.value );
    auto stat_it = statstbl.begin();
    eosio::check(statstbl.begin() != statstbl.end(), "Balance does not exist for user");
    eosio::check(stat_it->available.amount >= amount * IQ_PRECISION_MULTIPLIER, "not enough available IQ to vote");
    eosio::check(amount > 0, "PROTOCOL ERROR: Why is there a negative vote amount?");

    // Subtract amount from balance
    asset voting_iq = asset(amount * IQ_PRECISION_MULTIPLIER, IQSYMBOL);
    statstbl.modify( stat_it, same_payer, [&]( auto& g ) {
        g.available -= voting_iq;
        g.staked += voting_iq;
    });

    // Create the stake object
    staketbl staketblobj(_self, _self.value);
    uint64_t stake_id = staketblobj.available_primary_key();
    staketblobj.emplace( _self, [&]( auto& s ) {
        s.id = stake_id;
        s.user = voter;
        s.amount = amount;
        s.timestamp = eosio::current_time_point().sec_since_epoch();
        s.completion_time = eosio::current_time_point().sec_since_epoch() + STAKING_DURATION;
    });

    // mark if this is the initial editor vote
    votestbl votetbl( _self, proposal_id );
    bool voterIsProposer = (votetbl.begin() == votetbl.end()); 
   
    // Store vote in DB
    votetbl.emplace( _self, [&]( auto& a ) {
         a.id = votetbl.available_primary_key();
         a.proposal_id = proposal_id;
         a.approve = approve;
         a.is_editor = is_initial_vote;
         a.amount = round(amount * boost_multiplier);
         a.voter = voter;
         a.timestamp = eosio::current_time_point().sec_since_epoch();
         a.stake_id = stake_id;
         a.memo = memo;
    });
}

// Logic for proposing an edit for an article
// Users have to trigger this action through the everipediaiq::epartpropose action
[[eosio::action]]
void eparticlectr::propose2( name proposer, std::string slug, ipfshash_t ipfs_hash, std::string lang_code, int64_t group_id, std::string comment, std::string memo ) {
    require_auth( _self );

    // Table definition
    propstbl proptable( _self, _self.value );

    // Argument checks
    eosio::check( ipfs_hash.size() <= MAX_IPFS_SIZE, "IPFS hash is too long. MAX_IPFS_SIZE=46");
    eosio::check( ipfs_hash.size() >= MIN_IPFS_SIZE, "IPFS hash is too short. MIN_IPFS_SIZE=46");
    eosio::check( comment.size() < MAX_COMMENT_SIZE, "comment must be less than 256 bytes");
    eosio::check( memo.size() < MAX_MEMO_SIZE, "memo must be less than 32 bytes");
    eosio::check( group_id >= -1, "group_id must be greater than -2. Specify -1 for auto-generated ID");

    // Get the existing wiki_id or create an new one
    int64_t wiki_id = eparticlectr::get_or_create_wiki_id(_self, slug, lang_code);

    // Calculate proposal parameters
    uint64_t proposal_id = proptable.available_primary_key();
    uint32_t starttime = eosio::current_time_point().sec_since_epoch();
    uint32_t endtime = eosio::current_time_point().sec_since_epoch() + DEFAULT_VOTING_TIME;

    // Store the proposal
    proptable.emplace( _self, [&]( auto& p ) {
        p.id = proposal_id;
        p.wiki_id = wiki_id;
        p.slug = slug;
        p.group_id = group_id;
        p.lang_code = lang_code;
        p.ipfs_hash = ipfs_hash;
        p.proposer = proposer;
        p.starttime = starttime;
        p.endtime = endtime;
        p.memo = memo;
        p.finalized = false;
    });

    action(
        permission_level { _self, name("active") },
        _self, name("logpropinfo"),
        std::make_tuple( proposal_id, proposer, wiki_id, slug, ipfs_hash, lang_code, group_id, comment, memo, starttime, endtime )
    ).send();

    // Place the default vote
    action(
        permission_level { _self , name("active") },
        _self, name("vote"),
        std::make_tuple( proposer, proposal_id, true, EDIT_PROPOSE_IQ_EPARTICLECTR, std::string("editor initial vote"), memo )
    ).send();
}

[[eosio::action]]
void eparticlectr::finalize( uint64_t proposal_id ) {
    // Verify proposal exists
    propstbl proptable( _self, _self.value );
    auto prop_it = proptable.find( proposal_id );
    eosio::check( prop_it != proptable.end(), "Proposal not found" );

    // Verify voting period is complete
    eosio::check( eosio::current_time_point().sec_since_epoch() > prop_it->endtime, "Voting period is not over yet");

    // Retrieve votes from DB
    votestbl votetbl( _self, proposal_id );
    auto vote_it = votetbl.begin();
    eosio::check( vote_it != votetbl.end(), "No votes found for proposal");

    // Tally votes
    uint64_t yes_votes = 0;
    uint64_t no_votes = 0;
    while(vote_it->proposal_id == proposal_id && vote_it != votetbl.end()) {
        if (vote_it->approve)
            yes_votes += vote_it->amount;
        else
            no_votes += vote_it->amount;
        vote_it++;
    }

    // Determine slashing conditions
    vote_it = votetbl.begin();
    bool approved = 0;
    bool istie = 0;
    float totalVotes = yes_votes + no_votes;
    if ((yes_votes / totalVotes) >= TIER_ONE_THRESHOLD){
        approved = 1;
        if ((yes_votes / totalVotes) == TIER_ONE_THRESHOLD){
            istie = 1;
        }
    }
    float slash_ratio = 0.0f;
    if (approved){
        slash_ratio = (yes_votes - no_votes) / totalVotes;
    }
    else{
        slash_ratio = (no_votes - yes_votes) / totalVotes;
    }

    // Make sure no weird bugs cause the slash reward to under/overflow
    eosio::check( slash_ratio >= 0.0f && slash_ratio <= 1.0f, "Slash ratio out of bounds");

    rewardstbl rewardstable( _self, _self.value );
    staketbl staketable(_self, _self.value);

    // Slash / reward votes
    // Tally vote points
    uint64_t total_vote_points = 0;
    while(vote_it != votetbl.end() && istie == 0) {
        uint32_t extraSecsSlash = uint32_t((float)STAKING_DURATION * slash_ratio);
        
        // Refund winning editor stake immediately
        if (approved && vote_it->is_editor) {
            auto stake_it = staketable.find(vote_it->stake_id);
            staketable.modify( stake_it, same_payer, [&]( auto& a ) {
                a.completion_time = eosio::current_time_point().sec_since_epoch();
            });
        }
        
        // Slash losers
        if (vote_it->approve != approved) {
            auto stake_it = staketable.find(vote_it->stake_id);
            staketable.modify( stake_it, same_payer, [&]( auto& a ) {
                a.completion_time += extraSecsSlash;
            });

            action( 
                permission_level { _self, name("active") },
                _self, name("slashnotify"),
                std::make_tuple( vote_it->voter, vote_it->amount, extraSecsSlash, vote_it->memo )
            ).send();
        }
        // Reward the winners
        else{
            rewardstable.emplace( _self,  [&]( auto& a ) {
                a.id = rewardstable.available_primary_key();
                a.user = vote_it->voter;
                a.vote_points = vote_it->amount;
                if (approved && vote_it->is_editor)
                    a.edit_points = (yes_votes - no_votes);
                else
                    a.edit_points = 0;
                a.proposal_id = proposal_id;
                a.proposal_finalize_time = eosio::current_time_point().sec_since_epoch();
                a.proposal_finalize_period = uint32_t(eosio::current_time_point().sec_since_epoch() / REWARD_INTERVAL);
                a.proposalresult = approved;
                a.is_editor = vote_it->is_editor;
                a.is_tie = istie;
                a.memo = vote_it->memo;
            });
            total_vote_points += vote_it->amount;
        }
        vote_it++;
    }

    // Update rewards table
    uint64_t current_period = eosio::current_time_point().sec_since_epoch() / REWARD_INTERVAL;
    perrwdstbl perrewards( _self, _self.value );
    auto period_it = perrewards.find( current_period );
    uint64_t edit_points = approved ? (yes_votes - no_votes) : 0;
    if (period_it == perrewards.end()) {
        perrewards.emplace( _self, [&]( auto& p ) {
            p.period = current_period;
            p.curation_sum = total_vote_points;
            p.editor_sum = edit_points;
        });
    }
    else {
        perrewards.modify( period_it, same_payer, [&]( auto& p ) {
            p.curation_sum += total_vote_points;
            p.editor_sum += edit_points;
        });
    }

    // Insert wiki into DB
    if (approved) {
        wikistbl wikitbl( _self, _self.value );
        auto wiki_it = wikitbl.find( prop_it->wiki_id );
        eosio::check(wiki_it != wikitbl.end(), "PROTOCOL ERROR: An ID should already exist for this wiki");

        wikitbl.modify( wiki_it, _self, [&]( auto& a ) {
            a.slug = prop_it->slug;
            a.group_id = prop_it->group_id;
            a.lang_code = prop_it->lang_code;
            a.ipfs_hash = prop_it->ipfs_hash;
        });
    }

    // Log proposal result and new wiki id
    action(
        permission_level { _self, name("active") },
        _self, name("logpropres"),
        std::make_tuple( prop_it->id, approved, yes_votes, no_votes )
    ).send();

    // delete proposal if it's not the most current one
    // deleting the most current proposal screws up the ID auto-increment
    if (prop_it->id == proptable.available_primary_key() - 1)
        proptable.modify( prop_it, same_payer, [&]( auto& p ) {
            p.finalized = true;
        });
    else
        proptable.erase( prop_it );
}


[[eosio::action]]
void eparticlectr::rewardclmid ( uint64_t reward_id ) {
    // prep tables
    perrwdstbl perrewards( _self, _self.value );
    rewardstbl rewardstable( _self, _self.value );

    // check if user rewards exist
    auto reward_it = rewardstable.find( reward_id );
    eosio::check( reward_it != rewardstable.end(), "reward doesn't exist in database");

    // Make sure period is complete before processing
    uint64_t reward_period = reward_it->proposal_finalize_period;
    uint64_t current_period = eosio::current_time_point().sec_since_epoch() / REWARD_INTERVAL;
    auto period_it = perrewards.find( reward_period );
    eosio::check(period_it != perrewards.end(), "PROTOCOL ERROR: This period should exist");
    eosio::check(current_period > reward_period, "Reward period must complete before claiming rewards");

    // Send curation reward
    int64_t curation_reward = reward_it->vote_points * PERIOD_CURATION_REWARD / period_it->curation_sum;
    eosio::check(curation_reward <= PERIOD_CURATION_REWARD, "System logic error. Too much IQ calculated for curation reward.");
    if (curation_reward == 0) // Minimum reward of 0.001 IQ to prevent unclaimable rewards
        curation_reward = 1;
    asset curation_quantity = asset(curation_reward, IQSYMBOL);
    std::string memo = std::string("Curation IQ reward:" + reward_it->memo);
    action(
        permission_level { TOKEN_CONTRACT, name("active") },
        TOKEN_CONTRACT, name("issue"),
        std::make_tuple( reward_it->user, curation_quantity, memo )
    ).send();

    // Send editor reward
    if (reward_it->is_editor && reward_it->proposalresult) {
        int64_t editor_reward = reward_it->edit_points * PERIOD_EDITOR_REWARD / period_it->editor_sum;
        if (editor_reward == 0) // Minimum reward of 0.001 IQ to prevent unclaimable rewards
            editor_reward = 1;
        eosio::check(editor_reward <= PERIOD_EDITOR_REWARD, "System logic error. Too much IQ calculated for editor reward.");
        asset editor_quantity = asset(editor_reward, IQSYMBOL);
        std::string memo = std::string("Editor IQ reward:" + reward_it->memo);
        action(
            permission_level { TOKEN_CONTRACT, name("active") },
            TOKEN_CONTRACT, name("issue"),
            std::make_tuple( reward_it->user, editor_quantity, memo )
        ).send();
    }

    // delete reward after claiming
    rewardstable.erase( reward_it );
}

[[eosio::action]]
void eparticlectr::oldvotepurge( uint64_t proposal_id, uint32_t loop_limit ) {
    // Get the proposal object
    propstbl proptable( _self, _self.value );
    auto prop_it = proptable.find(proposal_id);
    eosio::check( prop_it == proptable.end() || prop_it->finalized, "Proposal not finalized yet!");

    // If it is an undeleted proposal, make sure it's no longer the most current proposal
    // so the auto-increment doesn't get screwed up
    // If it isn't delete it
    if ( prop_it != proptable.end() ) {
        eosio::check( prop_it->id != proptable.available_primary_key() - 1, "Cannot delete most recent proposal" );
        proptable.erase( prop_it );
    }

    // Retrieve votes from DB
    votestbl votetbl( _self, proposal_id );
    auto vote_it = votetbl.begin();
    eosio::check( vote_it != votetbl.end(), "No votes found for proposal!");

    // Free up RAM
    uint32_t count = 0;
    while(count < loop_limit && vote_it != votetbl.end()) {
        vote_it = votetbl.erase(vote_it);
        count++;
    }
}

[[eosio::action]]
void eparticlectr::mkreferendum( uint64_t proposal_id ) {
    propstbl proptable( _self, _self.value );

    // Validate proposal
    auto prop_it = proptable.find(proposal_id);
    eosio::check( prop_it != proptable.end(), "Proposal does not exist");
    eosio::check( !prop_it->finalized, "Proposal is finalized");
    eosio::check( prop_it->endtime - prop_it->starttime < REFERENDUM_DURATION_SECS, "Proposal has already been marked as a referendum");

    require_auth(prop_it->proposer);

    proptable.modify( prop_it, same_payer, [&]( auto& p ) {
        p.endtime = eosio::current_time_point().sec_since_epoch() + REFERENDUM_DURATION_SECS;
    });
}

[[eosio::action]]
void eparticlectr::slashnotify( name slashee, uint64_t amount, uint32_t seconds, std::string memo ){
    require_auth( _self );
}

[[eosio::action]]
void eparticlectr::logpropres( uint64_t proposal_id, bool approved, uint64_t yes_votes, uint64_t no_votes ) {
    require_auth( _self );
}

[[eosio::action]]
void eparticlectr::logpropinfo( uint64_t proposal_id, name proposer, uint64_t wiki_id, std::string slug, ipfshash_t ipfs_hash, std::string lang_code, uint64_t group_id, std::string comment, std::string memo, uint32_t starttime, uint32_t endtime) {
    require_auth( _self );
}

EOSIO_DISPATCH( eparticlectr, (boostinvest)(boosttxfr)(brainclmid)(slashnotify)(finalize)(oldvotepurge)(propose2)(rewardclmid)(vote)(logpropres)(logpropinfo)(mkreferendum) )
