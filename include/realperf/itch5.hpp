#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace realperf::itch5 {

constexpr std::size_t kMoldUdp64HeaderSize = 20;
constexpr std::size_t kSymbolSize = 8;
constexpr std::size_t kMpidSize = 4;
constexpr std::size_t kIssueSubTypeSize = 2;
constexpr std::size_t kReasonSize = 4;
constexpr std::size_t kTimestampSize = 6;
constexpr std::size_t kSystemEventSize = 12;
constexpr std::size_t kStockDirectorySize = 39;
constexpr std::size_t kStockTradingActionSize = 25;
constexpr std::size_t kRegShoRestrictionSize = 20;
constexpr std::size_t kMarketParticipantPositionSize = 26;
constexpr std::size_t kMwcbDeclineLevelSize = 35;
constexpr std::size_t kMwcbStatusSize = 12;
constexpr std::size_t kIpoQuotingPeriodUpdateSize = 28;
constexpr std::size_t kLuldAuctionCollarSize = 35;
constexpr std::size_t kOperationalHaltSize = 21;
constexpr std::size_t kAddOrderSize = 36;
constexpr std::size_t kAddOrderWithMpidSize = 40;
constexpr std::size_t kOrderExecutedSize = 31;
constexpr std::size_t kOrderExecutedWithPriceSize = 36;
constexpr std::size_t kOrderCancelSize = 23;
constexpr std::size_t kOrderDeleteSize = 19;
constexpr std::size_t kOrderReplaceSize = 35;
constexpr std::size_t kTradeSize = 44;
constexpr std::size_t kCrossTradeSize = 40;
constexpr std::size_t kBrokenTradeSize = 19;
constexpr std::size_t kNoiiSize = 50;
constexpr std::size_t kRetailPriceImprovementSize = 20;
constexpr std::size_t kDirectListingWithCapitalRaiseSize = 48;

enum class MessageType : char {
    system_event = 'S',
    stock_directory = 'R',
    stock_trading_action = 'H',
    reg_sho_restriction = 'Y',
    market_participant_position = 'L',
    mwcb_decline_level = 'V',
    mwcb_status = 'W',
    ipo_quoting_period_update = 'K',
    luld_auction_collar = 'J',
    operational_halt = 'h',
    add_order = 'A',
    add_order_with_mpid = 'F',
    order_executed = 'E',
    order_executed_with_price = 'C',
    order_cancel = 'X',
    order_delete = 'D',
    order_replace = 'U',
    trade = 'P',
    cross_trade = 'Q',
    broken_trade = 'B',
    noii = 'I',
    retail_price_improvement = 'N',
    direct_listing_with_capital_raise = 'O',
};

#pragma pack(push, 1)

struct Header {
    char type;
    std::uint16_t locate;
    std::uint16_t tracking;
    std::uint8_t timestamp[kTimestampSize];
};

struct SystemEvent : Header {
    char event_code;
};

struct StockDirectory : Header {
    char stock[kSymbolSize];
    char market_category;
    char financial_status_indicator;
    std::uint32_t round_lot_size;
    char round_lots_only;
    char issue_classification;
    char issue_sub_type[kIssueSubTypeSize];
    char authenticity;
    char short_sale_threshold_indicator;
    char ipo_flag;
    char luld_reference_price_tier;
    char etp_flag;
    std::uint32_t etp_leverage_factor;
    char inverse_indicator;
};

struct StockTradingAction : Header {
    char stock[kSymbolSize];
    char trading_state;
    char reserved;
    char reason[kReasonSize];
};

struct RegShoRestriction : Header {
    char stock[kSymbolSize];
    char action;
};

struct MarketParticipantPosition : Header {
    char mpid[kMpidSize];
    char stock[kSymbolSize];
    char primary_market_maker;
    char market_maker_mode;
    char market_participant_state;
};

struct MwcbDeclineLevel : Header {
    std::uint64_t level_1;
    std::uint64_t level_2;
    std::uint64_t level_3;
};

struct MwcbStatus : Header {
    char breached_level;
};

struct IpoQuotingPeriodUpdate : Header {
    char stock[kSymbolSize];
    std::uint32_t quotation_release_time;
    char quotation_release_qualifier;
    std::uint32_t price;
};

struct LuldAuctionCollar : Header {
    char stock[kSymbolSize];
    std::uint32_t reference_price;
    std::uint32_t upper_collar_price;
    std::uint32_t lower_collar_price;
    std::uint32_t extension;
};

struct OperationalHalt : Header {
    char stock[kSymbolSize];
    char market_code;
    char action;
};

struct OrderAdd : Header {
    std::uint64_t order_ref;
    char side;
    std::uint32_t shares;
    char stock[kSymbolSize];
    std::uint32_t price;
};

struct OrderAddWithMpid : Header {
    std::uint64_t order_ref;
    char side;
    std::uint32_t shares;
    char stock[kSymbolSize];
    std::uint32_t price;
    char attribution[kMpidSize];
};

struct OrderExecute : Header {
    std::uint64_t order_ref;
    std::uint32_t shares;
    std::uint64_t match;
};

struct OrderExecuteWithPrice : Header {
    std::uint64_t order_ref;
    std::uint32_t shares;
    std::uint64_t match;
    char printable;
    std::uint32_t price;
};

struct OrderCancel : Header {
    std::uint64_t order_ref;
    std::uint32_t canceled_shares;
};

struct OrderDelete : Header {
    std::uint64_t order_ref;
};

struct OrderReplace : Header {
    std::uint64_t original_order_ref;
    std::uint64_t new_order_ref;
    std::uint32_t shares;
    std::uint32_t price;
};

struct Trade : Header {
    std::uint64_t order_ref;
    char side;
    std::uint32_t shares;
    char stock[kSymbolSize];
    std::uint32_t price;
    std::uint64_t match;
};

struct CrossTrade : Header {
    std::uint64_t shares;
    char stock[kSymbolSize];
    std::uint32_t price;
    std::uint64_t match;
    char cross_type;
};

struct BrokenTrade : Header {
    std::uint64_t match;
};

struct Noii : Header {
    std::uint64_t paired_shares;
    std::uint64_t imbalance_shares;
    char imbalance_direction;
    char stock[kSymbolSize];
    std::uint32_t far_price;
    std::uint32_t near_price;
    std::uint32_t current_reference_price;
    char cross_type;
    char price_variation_indicator;
};

struct RetailPriceImprovement : Header {
    char stock[kSymbolSize];
    char interest_flag;
};

struct DirectListingWithCapitalRaise : Header {
    char stock[kSymbolSize];
    char open_eligibility_status;
    std::uint32_t minimum_allowable_price;
    std::uint32_t maximum_allowable_price;
    std::uint32_t near_execution_price;
    std::uint64_t near_execution_time;
    std::uint32_t lower_price_range_collar;
    std::uint32_t upper_price_range_collar;
};

#pragma pack(pop)

static_assert(sizeof(Header) == 11);
static_assert(sizeof(SystemEvent) == kSystemEventSize);
static_assert(sizeof(StockDirectory) == kStockDirectorySize);
static_assert(sizeof(StockTradingAction) == kStockTradingActionSize);
static_assert(sizeof(RegShoRestriction) == kRegShoRestrictionSize);
static_assert(sizeof(MarketParticipantPosition) == kMarketParticipantPositionSize);
static_assert(sizeof(MwcbDeclineLevel) == kMwcbDeclineLevelSize);
static_assert(sizeof(MwcbStatus) == kMwcbStatusSize);
static_assert(sizeof(IpoQuotingPeriodUpdate) == kIpoQuotingPeriodUpdateSize);
static_assert(sizeof(LuldAuctionCollar) == kLuldAuctionCollarSize);
static_assert(sizeof(OperationalHalt) == kOperationalHaltSize);
static_assert(sizeof(OrderAdd) == kAddOrderSize);
static_assert(sizeof(OrderAddWithMpid) == kAddOrderWithMpidSize);
static_assert(sizeof(OrderExecute) == kOrderExecutedSize);
static_assert(sizeof(OrderExecuteWithPrice) == kOrderExecutedWithPriceSize);
static_assert(sizeof(OrderCancel) == kOrderCancelSize);
static_assert(sizeof(OrderDelete) == kOrderDeleteSize);
static_assert(sizeof(OrderReplace) == kOrderReplaceSize);
static_assert(sizeof(Trade) == kTradeSize);
static_assert(sizeof(CrossTrade) == kCrossTradeSize);
static_assert(sizeof(BrokenTrade) == kBrokenTradeSize);
static_assert(sizeof(Noii) == kNoiiSize);
static_assert(sizeof(RetailPriceImprovement) == kRetailPriceImprovementSize);
static_assert(sizeof(DirectListingWithCapitalRaise) == kDirectListingWithCapitalRaiseSize);

static_assert(std::is_trivially_copyable_v<Header>);
static_assert(std::is_trivially_copyable_v<SystemEvent>);
static_assert(std::is_trivially_copyable_v<StockDirectory>);
static_assert(std::is_trivially_copyable_v<StockTradingAction>);
static_assert(std::is_trivially_copyable_v<RegShoRestriction>);
static_assert(std::is_trivially_copyable_v<MarketParticipantPosition>);
static_assert(std::is_trivially_copyable_v<MwcbDeclineLevel>);
static_assert(std::is_trivially_copyable_v<MwcbStatus>);
static_assert(std::is_trivially_copyable_v<IpoQuotingPeriodUpdate>);
static_assert(std::is_trivially_copyable_v<LuldAuctionCollar>);
static_assert(std::is_trivially_copyable_v<OperationalHalt>);
static_assert(std::is_trivially_copyable_v<OrderAdd>);
static_assert(std::is_trivially_copyable_v<OrderAddWithMpid>);
static_assert(std::is_trivially_copyable_v<OrderExecute>);
static_assert(std::is_trivially_copyable_v<OrderExecuteWithPrice>);
static_assert(std::is_trivially_copyable_v<OrderCancel>);
static_assert(std::is_trivially_copyable_v<OrderDelete>);
static_assert(std::is_trivially_copyable_v<OrderReplace>);
static_assert(std::is_trivially_copyable_v<Trade>);
static_assert(std::is_trivially_copyable_v<CrossTrade>);
static_assert(std::is_trivially_copyable_v<BrokenTrade>);
static_assert(std::is_trivially_copyable_v<Noii>);
static_assert(std::is_trivially_copyable_v<RetailPriceImprovement>);
static_assert(std::is_trivially_copyable_v<DirectListingWithCapitalRaise>);

static_assert(alignof(Header) == 1);
static_assert(alignof(SystemEvent) == 1);
static_assert(alignof(StockDirectory) == 1);
static_assert(alignof(StockTradingAction) == 1);
static_assert(alignof(RegShoRestriction) == 1);
static_assert(alignof(MarketParticipantPosition) == 1);
static_assert(alignof(MwcbDeclineLevel) == 1);
static_assert(alignof(MwcbStatus) == 1);
static_assert(alignof(IpoQuotingPeriodUpdate) == 1);
static_assert(alignof(LuldAuctionCollar) == 1);
static_assert(alignof(OperationalHalt) == 1);
static_assert(alignof(OrderAdd) == 1);
static_assert(alignof(OrderAddWithMpid) == 1);
static_assert(alignof(OrderExecute) == 1);
static_assert(alignof(OrderExecuteWithPrice) == 1);
static_assert(alignof(OrderCancel) == 1);
static_assert(alignof(OrderDelete) == 1);
static_assert(alignof(OrderReplace) == 1);
static_assert(alignof(Trade) == 1);
static_assert(alignof(CrossTrade) == 1);
static_assert(alignof(BrokenTrade) == 1);
static_assert(alignof(Noii) == 1);
static_assert(alignof(RetailPriceImprovement) == 1);
static_assert(alignof(DirectListingWithCapitalRaise) == 1);

static_assert(static_cast<Header*>(static_cast<SystemEvent*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<StockDirectory*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<StockTradingAction*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<RegShoRestriction*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<MarketParticipantPosition*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<MwcbDeclineLevel*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<MwcbStatus*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<IpoQuotingPeriodUpdate*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<LuldAuctionCollar*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<OperationalHalt*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<OrderAdd*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<OrderAddWithMpid*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<OrderExecute*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<OrderExecuteWithPrice*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<OrderCancel*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<OrderDelete*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<OrderReplace*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<Trade*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<CrossTrade*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<BrokenTrade*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<Noii*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<RetailPriceImprovement*>(nullptr)) == nullptr);
static_assert(static_cast<Header*>(static_cast<DirectListingWithCapitalRaise*>(nullptr)) == nullptr);

struct Message {
    const Header* header = nullptr;
    std::size_t extra_bytes = 0;
};

template <typename UInt>
inline UInt from_big_endian(UInt value)
{
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(value);
    } else {
        return value;
    }
}

inline void put_u16_be(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    value = from_big_endian(value);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(value));
}

inline void put_u32_be(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    value = from_big_endian(value);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(value));
}

inline void put_u48_be(std::vector<std::uint8_t>& out, std::uint64_t value)
{
    value &= 0x0000FFFFFFFFFFFFULL;
    if constexpr (std::endian::native == std::endian::little) {
        value = std::byteswap(value) >> 16;
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        out.insert(out.end(), bytes, bytes + kTimestampSize);
    } else {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        out.insert(out.end(), bytes + 2, bytes + sizeof(value));
    }
}

inline void put_u64_be(std::vector<std::uint8_t>& out, std::uint64_t value)
{
    value = from_big_endian(value);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(value));
}

inline std::uint16_t read_u16_be(const std::uint8_t* data)
{
    std::uint16_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return from_big_endian(value);
}

inline std::uint32_t read_u32_be(const std::uint8_t* data)
{
    std::uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return from_big_endian(value);
}

inline std::uint64_t read_u48_be(const std::uint8_t* data)
{
    std::uint64_t value = 0;
    if constexpr (std::endian::native == std::endian::little) {
        std::memcpy(&value, data, kTimestampSize);
        return std::byteswap(value) >> 16;
    } else {
        std::memcpy(reinterpret_cast<std::uint8_t*>(&value) + 2, data, kTimestampSize);
        return value;
    }
}

inline std::uint64_t read_u64_be(const std::uint8_t* data)
{
    std::uint64_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return from_big_endian(value);
}

template <typename UInt>
inline UInt read_be(UInt value)
{
    return from_big_endian(value);
}

inline std::uint64_t read_timestamp(const std::uint8_t (&timestamp)[kTimestampSize])
{
    return read_u48_be(timestamp);
}

inline void print_alpha(std::ostream& out, const char* value, std::size_t size)
{
    while (size > 0 && value[size - 1] == ' ') {
        --size;
    }
    out.write(value, static_cast<std::streamsize>(size));
}

inline void put_alpha(std::vector<std::uint8_t>& out, std::string_view value, std::size_t size)
{
    const auto copied = std::min(value.size(), size);
    out.insert(out.end(), value.begin(), value.begin() + static_cast<std::ptrdiff_t>(copied));
    out.insert(out.end(), size - copied, static_cast<std::uint8_t>(' '));
}

inline std::vector<std::uint8_t> make_system_event(std::uint16_t locate,
                                                   std::uint16_t tracking,
                                                   std::uint64_t timestamp,
                                                   char code)
{
    std::vector<std::uint8_t> msg;
    msg.reserve(kSystemEventSize);
    msg.push_back(static_cast<char>(MessageType::system_event));
    put_u16_be(msg, locate);
    put_u16_be(msg, tracking);
    put_u48_be(msg, timestamp);
    msg.push_back(static_cast<std::uint8_t>(code));
    return msg;
}

inline std::vector<std::uint8_t> make_add_order(std::uint16_t locate,
                                                std::uint16_t tracking,
                                                std::uint64_t timestamp,
                                                std::uint64_t order_ref,
                                                char side,
                                                std::uint32_t shares,
                                                std::string_view symbol,
                                                std::uint32_t price)
{
    std::vector<std::uint8_t> msg;
    msg.reserve(kAddOrderSize);
    msg.push_back(static_cast<char>(MessageType::add_order));
    put_u16_be(msg, locate);
    put_u16_be(msg, tracking);
    put_u48_be(msg, timestamp);
    put_u64_be(msg, order_ref);
    msg.push_back(static_cast<std::uint8_t>(side));
    put_u32_be(msg, shares);
    put_alpha(msg, symbol, kSymbolSize);
    put_u32_be(msg, price);
    return msg;
}

inline std::vector<std::uint8_t> make_order_executed(std::uint16_t locate,
                                                     std::uint16_t tracking,
                                                     std::uint64_t timestamp,
                                                     std::uint64_t order_ref,
                                                     std::uint32_t shares,
                                                     std::uint64_t match)
{
    std::vector<std::uint8_t> msg;
    msg.reserve(kOrderExecutedSize);
    msg.push_back(static_cast<char>(MessageType::order_executed));
    put_u16_be(msg, locate);
    put_u16_be(msg, tracking);
    put_u48_be(msg, timestamp);
    put_u64_be(msg, order_ref);
    put_u32_be(msg, shares);
    put_u64_be(msg, match);
    return msg;
}

inline std::vector<std::uint8_t> make_order_delete(std::uint16_t locate,
                                                   std::uint16_t tracking,
                                                   std::uint64_t timestamp,
                                                   std::uint64_t order_ref)
{
    std::vector<std::uint8_t> msg;
    msg.reserve(kOrderDeleteSize);
    msg.push_back(static_cast<char>(MessageType::order_delete));
    put_u16_be(msg, locate);
    put_u16_be(msg, tracking);
    put_u48_be(msg, timestamp);
    put_u64_be(msg, order_ref);
    return msg;
}

inline std::vector<std::uint8_t> make_trade(std::uint16_t locate,
                                            std::uint16_t tracking,
                                            std::uint64_t timestamp,
                                            std::uint64_t order_ref,
                                            char side,
                                            std::uint32_t shares,
                                            std::string_view symbol,
                                            std::uint32_t price,
                                            std::uint64_t match)
{
    std::vector<std::uint8_t> msg;
    msg.reserve(kTradeSize);
    msg.push_back(static_cast<char>(MessageType::trade));
    put_u16_be(msg, locate);
    put_u16_be(msg, tracking);
    put_u48_be(msg, timestamp);
    put_u64_be(msg, order_ref);
    msg.push_back(static_cast<std::uint8_t>(side));
    put_u32_be(msg, shares);
    put_alpha(msg, symbol, kSymbolSize);
    put_u32_be(msg, price);
    put_u64_be(msg, match);
    return msg;
}

inline std::size_t message_size(char type)
{
    switch (static_cast<MessageType>(type)) {
    case MessageType::system_event: return kSystemEventSize;
    case MessageType::stock_directory: return kStockDirectorySize;
    case MessageType::stock_trading_action: return kStockTradingActionSize;
    case MessageType::reg_sho_restriction: return kRegShoRestrictionSize;
    case MessageType::market_participant_position: return kMarketParticipantPositionSize;
    case MessageType::mwcb_decline_level: return kMwcbDeclineLevelSize;
    case MessageType::mwcb_status: return kMwcbStatusSize;
    case MessageType::ipo_quoting_period_update: return kIpoQuotingPeriodUpdateSize;
    case MessageType::luld_auction_collar: return kLuldAuctionCollarSize;
    case MessageType::operational_halt: return kOperationalHaltSize;
    case MessageType::add_order: return kAddOrderSize;
    case MessageType::add_order_with_mpid: return kAddOrderWithMpidSize;
    case MessageType::order_executed: return kOrderExecutedSize;
    case MessageType::order_executed_with_price: return kOrderExecutedWithPriceSize;
    case MessageType::order_cancel: return kOrderCancelSize;
    case MessageType::order_delete: return kOrderDeleteSize;
    case MessageType::order_replace: return kOrderReplaceSize;
    case MessageType::trade: return kTradeSize;
    case MessageType::cross_trade: return kCrossTradeSize;
    case MessageType::broken_trade: return kBrokenTradeSize;
    case MessageType::noii: return kNoiiSize;
    case MessageType::retail_price_improvement: return kRetailPriceImprovementSize;
    case MessageType::direct_listing_with_capital_raise: return kDirectListingWithCapitalRaiseSize;
    default: return sizeof(Header);
    }
}

inline Message parse_message(const std::uint8_t* data, std::size_t size)
{
    if (size < sizeof(Header)) {
        throw std::runtime_error("truncated ITCH message");
    }

    const auto* header = reinterpret_cast<const Header*>(data);
    const auto expected = message_size(header->type);
    if (size < expected) {
        throw std::runtime_error("truncated ITCH message");
    }

    return Message{header, size - expected};
}

inline void print_price(std::ostream& out, std::uint32_t price)
{
    out << (price / 10000) << '.'
        << std::setfill('0') << std::setw(4) << (price % 10000)
        << std::setfill(' ');
}

template <typename MessageT>
inline const MessageT& as_message(const Header& header)
{
    return *reinterpret_cast<const MessageT*>(&header);
}

inline std::ostream& operator<<(std::ostream& out, const Header& header)
{
    out << "type=" << header.type
        << " locate=" << read_be(header.locate)
        << " tracking=" << read_be(header.tracking)
        << " ts_ns=" << read_timestamp(header.timestamp);

    switch (static_cast<MessageType>(header.type)) {
    case MessageType::system_event: {
        const auto& event = as_message<SystemEvent>(header);
        return out << " system_event=" << event.event_code;
    }
    case MessageType::stock_directory: {
        const auto& directory = as_message<StockDirectory>(header);
        out << " stock_directory stock=";
        print_alpha(out, directory.stock, kSymbolSize);
        out << " market_category=" << directory.market_category
            << " financial_status=" << directory.financial_status_indicator
            << " round_lot_size=" << read_be(directory.round_lot_size)
            << " round_lots_only=" << directory.round_lots_only
            << " issue_class=" << directory.issue_classification
            << " issue_sub_type=";
        print_alpha(out, directory.issue_sub_type, kIssueSubTypeSize);
        return out << " authenticity=" << directory.authenticity
                   << " short_sale_threshold=" << directory.short_sale_threshold_indicator
                   << " ipo_flag=" << directory.ipo_flag
                   << " luld_tier=" << directory.luld_reference_price_tier
                   << " etp_flag=" << directory.etp_flag
                   << " etp_leverage=" << read_be(directory.etp_leverage_factor)
                   << " inverse=" << directory.inverse_indicator;
    }
    case MessageType::stock_trading_action: {
        const auto& action = as_message<StockTradingAction>(header);
        out << " stock_trading_action stock=";
        print_alpha(out, action.stock, kSymbolSize);
        out << " state=" << action.trading_state << " reason=";
        print_alpha(out, action.reason, kReasonSize);
        return out;
    }
    case MessageType::reg_sho_restriction: {
        const auto& restriction = as_message<RegShoRestriction>(header);
        out << " reg_sho stock=";
        print_alpha(out, restriction.stock, kSymbolSize);
        return out << " action=" << restriction.action;
    }
    case MessageType::market_participant_position: {
        const auto& position = as_message<MarketParticipantPosition>(header);
        out << " market_participant_position mpid=";
        print_alpha(out, position.mpid, kMpidSize);
        out << " stock=";
        print_alpha(out, position.stock, kSymbolSize);
        return out << " primary_market_maker=" << position.primary_market_maker
                   << " mode=" << position.market_maker_mode
                   << " state=" << position.market_participant_state;
    }
    case MessageType::mwcb_decline_level: {
        const auto& levels = as_message<MwcbDeclineLevel>(header);
        return out << " mwcb_decline_level level_1=" << read_be(levels.level_1)
                   << " level_2=" << read_be(levels.level_2)
                   << " level_3=" << read_be(levels.level_3);
    }
    case MessageType::mwcb_status: {
        const auto& status = as_message<MwcbStatus>(header);
        return out << " mwcb_status breached_level=" << status.breached_level;
    }
    case MessageType::ipo_quoting_period_update: {
        const auto& update = as_message<IpoQuotingPeriodUpdate>(header);
        out << " ipo_quoting_period_update stock=";
        print_alpha(out, update.stock, kSymbolSize);
        out << " release_time=" << read_be(update.quotation_release_time)
            << " qualifier=" << update.quotation_release_qualifier
            << " price=";
        print_price(out, read_be(update.price));
        return out;
    }
    case MessageType::luld_auction_collar: {
        const auto& collar = as_message<LuldAuctionCollar>(header);
        out << " luld_auction_collar stock=";
        print_alpha(out, collar.stock, kSymbolSize);
        out << " reference_price=";
        print_price(out, read_be(collar.reference_price));
        out << " upper_collar=";
        print_price(out, read_be(collar.upper_collar_price));
        out << " lower_collar=";
        print_price(out, read_be(collar.lower_collar_price));
        return out << " extension=" << read_be(collar.extension);
    }
    case MessageType::operational_halt: {
        const auto& halt = as_message<OperationalHalt>(header);
        out << " operational_halt stock=";
        print_alpha(out, halt.stock, kSymbolSize);
        return out << " market_code=" << halt.market_code
                   << " action=" << halt.action;
    }
    case MessageType::add_order: {
        const auto& add = as_message<OrderAdd>(header);
        out << " add_order ref=" << read_be(add.order_ref)
            << " side=" << add.side
            << " shares=" << read_be(add.shares)
            << " stock=";
        print_alpha(out, add.stock, kSymbolSize);
        out << " price=";
        print_price(out, read_be(add.price));
        return out;
    }
    case MessageType::add_order_with_mpid: {
        const auto& add = as_message<OrderAddWithMpid>(header);
        out << " add_order_with_mpid ref=" << read_be(add.order_ref)
            << " side=" << add.side
            << " shares=" << read_be(add.shares)
            << " stock=";
        print_alpha(out, add.stock, kSymbolSize);
        out << " price=";
        print_price(out, read_be(add.price));
        out << " attribution=";
        print_alpha(out, add.attribution, kMpidSize);
        return out;
    }
    case MessageType::order_executed: {
        const auto& executed = as_message<OrderExecute>(header);
        return out << " executed ref=" << read_be(executed.order_ref)
                   << " shares=" << read_be(executed.shares)
                   << " match=" << read_be(executed.match);
    }
    case MessageType::order_executed_with_price: {
        const auto& executed = as_message<OrderExecuteWithPrice>(header);
        out << " executed_with_price ref=" << read_be(executed.order_ref)
            << " shares=" << read_be(executed.shares)
            << " match=" << read_be(executed.match)
            << " printable=" << executed.printable
            << " price=";
        print_price(out, read_be(executed.price));
        return out;
    }
    case MessageType::order_cancel: {
        const auto& cancel = as_message<OrderCancel>(header);
        return out << " cancel ref=" << read_be(cancel.order_ref)
                   << " canceled_shares=" << read_be(cancel.canceled_shares);
    }
    case MessageType::order_delete: {
        const auto& deleted = as_message<OrderDelete>(header);
        return out << " delete ref=" << read_be(deleted.order_ref);
    }
    case MessageType::order_replace: {
        const auto& replace = as_message<OrderReplace>(header);
        out << " replace original_ref=" << read_be(replace.original_order_ref)
            << " new_ref=" << read_be(replace.new_order_ref)
            << " shares=" << read_be(replace.shares)
            << " price=";
        print_price(out, read_be(replace.price));
        return out;
    }
    case MessageType::trade: {
        const auto& trade = as_message<Trade>(header);
        out << " trade ref=" << read_be(trade.order_ref)
            << " side=" << trade.side
            << " shares=" << read_be(trade.shares)
            << " stock=";
        print_alpha(out, trade.stock, kSymbolSize);
        out << " price=";
        print_price(out, read_be(trade.price));
        return out << " match=" << read_be(trade.match);
    }
    case MessageType::cross_trade: {
        const auto& trade = as_message<CrossTrade>(header);
        out << " cross_trade shares=" << read_be(trade.shares)
            << " stock=";
        print_alpha(out, trade.stock, kSymbolSize);
        out << " price=";
        print_price(out, read_be(trade.price));
        return out << " match=" << read_be(trade.match)
                   << " cross_type=" << trade.cross_type;
    }
    case MessageType::broken_trade: {
        const auto& broken = as_message<BrokenTrade>(header);
        return out << " broken_trade match=" << read_be(broken.match);
    }
    case MessageType::noii: {
        const auto& noii = as_message<Noii>(header);
        out << " noii paired_shares=" << read_be(noii.paired_shares)
            << " imbalance_shares=" << read_be(noii.imbalance_shares)
            << " direction=" << noii.imbalance_direction
            << " stock=";
        print_alpha(out, noii.stock, kSymbolSize);
        out << " far_price=";
        print_price(out, read_be(noii.far_price));
        out << " near_price=";
        print_price(out, read_be(noii.near_price));
        out << " current_reference_price=";
        print_price(out, read_be(noii.current_reference_price));
        return out << " cross_type=" << noii.cross_type
                   << " price_variation=" << noii.price_variation_indicator;
    }
    case MessageType::retail_price_improvement: {
        const auto& improvement = as_message<RetailPriceImprovement>(header);
        out << " retail_price_improvement stock=";
        print_alpha(out, improvement.stock, kSymbolSize);
        return out << " interest=" << improvement.interest_flag;
    }
    case MessageType::direct_listing_with_capital_raise: {
        const auto& dlcr = as_message<DirectListingWithCapitalRaise>(header);
        out << " direct_listing_with_capital_raise stock=";
        print_alpha(out, dlcr.stock, kSymbolSize);
        out << " open_eligibility=" << dlcr.open_eligibility_status
            << " min_price=";
        print_price(out, read_be(dlcr.minimum_allowable_price));
        out << " max_price=";
        print_price(out, read_be(dlcr.maximum_allowable_price));
        out << " near_execution_price=";
        print_price(out, read_be(dlcr.near_execution_price));
        out << " near_execution_time=" << read_be(dlcr.near_execution_time)
            << " lower_collar=";
        print_price(out, read_be(dlcr.lower_price_range_collar));
        out << " upper_collar=";
        print_price(out, read_be(dlcr.upper_price_range_collar));
        return out;
    }
    default:
        return out << " unsupported";
    }
}

inline std::ostream& operator<<(std::ostream& out, const Message& message)
{
    if (message.header == nullptr) {
        return out << "null ITCH message";
    }
    return out << *message.header;
}

} // namespace realperf::itch5
