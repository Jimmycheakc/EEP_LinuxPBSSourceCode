#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <queue>
#include <vector>
#include <unordered_map>
#include "tcp_client.h"


class EEPClient
{
public:

    enum class PRIORITY : int
    {
        UNSOLICITED     = 0,
        WATCHDOG        = 1,
        NORMAL          = 2,
        HEALTHSTATUS    = 3
    };

    enum class MSG_STATUS : uint32_t
    {
        // General Status
        SUCCESS         = 0x00000000u,
        SEND_FAILED     = 0x00000001u,
        RSP_TIMEOUT     = 0x00000003u,
        PARSE_FAILED    = 0x00000004u
    };

    enum class MESSAGE_CODE : uint8_t
    {
        ACK                                                 = 0x10,
        NAK                                                 = 0x11,
        HEALTH_STATUS_REQUEST                               = 0x72,
        HEALTH_STATUS_RESPONSE                              = 0x13,
        WATCHDOG_REQUEST                                    = 0x73,
        WATCHDOG_RESPONSE                                   = 0x15,
        START_REQUEST                                       = 0x40,
        START_RESPONSE                                      = 0x41,
        STOP_REQUEST                                        = 0x42,
        STOP_RESPONSE                                       = 0x43,
        DI_STATUS_NOTIFICATION                              = 0x60,
        DI_STATUS_REQUEST                                   = 0x61,
        DI_STATUS_RESPONSE                                  = 0x62,
        SET_DO_REQUEST                                      = 0x63,
        SET_DI_PORT_CONFIGURATION                           = 0x64,
        GET_OBU_INFORMATION_REQUEST                         = 0x20,
        OBU_INFORMATION_NOTIFICATION                        = 0x21,
        GET_OBU_INFORMATION_STOP                            = 0x22,
        DEDUCT_REQUEST                                      = 0x82,
        TRANSACTION_DATA                                    = 0xF4,
        DEDUCT_STOP_REQUEST                                 = 0x83,
        TRANSACTION_REQUEST                                 = 0x76,
        CPO_INFORMATION_DISPLAY_REQUEST                     = 0x30,
        CPO_INFORMATION_DISPLAY_RESULT                      = 0x31,
        CARPARK_PROCESS_COMPLETE_NOTIFICATION               = 0x32,
        CARPARK_PROCESS_COMPLETE_RESULT                     = 0x37,
        DSRC_PROCESS_COMPLETE_NOTIFICATION                  = 0x33,
        STOP_REQUEST_OF_RELATED_INFORMATION_DISTRIBUTION    = 0x34,
        DSRC_STATUS_REQUEST                                 = 0x35,
        DSRC_STATUS_RESPONSE                                = 0x36,
        TIME_CALIBRATION_REQUEST                            = 0x50,
        TIME_CALIBRATION_RESPONSE                           = 0x51,
        EEP_RESTART_INQUIRY                                 = 0x53,
        EEP_RESTART_INQUIRY_RESPONSE                        = 0x54,
        NOTIFICATION_LOG                                    = 0xF2,
        SET_PARKING_AVAILABLE                               = 0x55,
        CD_DOWNLOAD_REQUEST                                 = 0x93
    };
    
    enum class STATE
    {
        IDLE,
        CONNECTING,
        CONNECTED,
        WRITING_REQUEST,
        WAITING_FOR_RESPONSE,
        RECONNECT,
        STATE_COUNT
    };

    enum class EVENT
    {
        CONNECT,
        CONNECT_SUCCESS,
        CONNECT_FAIL,
        CHECK_COMMAND,
        WRITE_COMMAND,
        WRITE_COMPLETED,
        WRITE_TIMEOUT,
        RESPONSE_TIMER_CANCELLED_RSP_RECEIVED,
        RESPONSE_TIMEOUT,
        RECONNECT_REQUEST,
        UNSOLICITED_REQUEST_DONE,
        EVENT_COUNT
    };

    struct EventTransition
    {
        EVENT event;
        void (EEPClient::*eventHandler)(EVENT);
        STATE nextState;
    };

    struct StateTransition
    {
        STATE stateName;
        std::vector<EventTransition> transitions;
    };

    struct CommandDataBase
    {
        virtual ~CommandDataBase() = default;
        virtual std::vector<uint8_t> serialize() const = 0;
    };

    struct AckData : CommandDataBase
    {
        uint8_t reqDatatypeCode;
        uint8_t rsv[3];

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.push_back(reqDatatypeCode);
            out.insert(out.end(), std::begin(rsv), std::end(rsv));

            return out;
        }
    };

    struct NakData : CommandDataBase
    {
        uint8_t reqDatatypeCode;
        uint8_t reasonCode;
        uint8_t rsv[2];

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.push_back(reqDatatypeCode);
            out.push_back(reasonCode);
            out.insert(out.end(), std::begin(rsv), std::end(rsv));

            return out;
        }
    };

    struct SetDIPortConfigData : CommandDataBase
    {
        uint8_t periodDebounceDI1[2];
        uint8_t periodDebounceDI2[2];
        uint8_t periodDebounceDI3[2];
        uint8_t periodDebounceDI4[2];
        uint8_t periodDebounceDI5[2];
        uint8_t rsv[2];

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.insert(out.end(), std::begin(periodDebounceDI1), std::end(periodDebounceDI1));
            out.insert(out.end(), std::begin(periodDebounceDI2), std::end(periodDebounceDI2));
            out.insert(out.end(), std::begin(periodDebounceDI3), std::end(periodDebounceDI3));
            out.insert(out.end(), std::begin(periodDebounceDI4), std::end(periodDebounceDI4));
            out.insert(out.end(), std::begin(periodDebounceDI5), std::end(periodDebounceDI5));
            out.insert(out.end(), std::begin(rsv), std::end(rsv));

            return out;
        }
    };

    struct DeductData : CommandDataBase
    {
        uint8_t serialNum[2];
        uint8_t rsv[10];
        uint8_t obuLabel[5];
        uint8_t rsv1[3];
        uint8_t chargeAmt[2];
        uint8_t rsv2[2];
        uint8_t parkingStartDay;
        uint8_t parkingStartMonth;
        uint8_t parkingStartYear[2];
        uint8_t rsv3;
        uint8_t parkingStartSecond;
        uint8_t parkingStartMinute;
        uint8_t parkingStartHour;
        uint8_t parkingEndDay;
        uint8_t parkingEndMonth;
        uint8_t parkingEndYear[2];
        uint8_t rsv4;
        uint8_t parkingEndSecond;
        uint8_t parkingEndMinute;
        uint8_t parkingEndHour;

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.insert(out.end(), std::begin(serialNum), std::end(serialNum));
            out.insert(out.end(), std::begin(rsv), std::end(rsv));
            out.insert(out.end(), std::begin(obuLabel), std::end(obuLabel));
            out.insert(out.end(), std::begin(rsv1), std::end(rsv1));
            out.insert(out.end(), std::begin(chargeAmt), std::end(chargeAmt));
            out.insert(out.end(), std::begin(rsv2), std::end(rsv2));
            out.push_back(parkingStartDay);
            out.push_back(parkingStartMonth);
            out.insert(out.end(), std::begin(parkingStartYear), std::end(parkingStartYear));
            out.push_back(rsv3);
            out.push_back(parkingStartSecond);
            out.push_back(parkingStartMinute);
            out.push_back(parkingStartHour);
            out.push_back(parkingEndDay);
            out.push_back(parkingEndMonth);
            out.insert(out.end(), std::begin(parkingEndYear), std::end(parkingEndYear));
            out.push_back(rsv4);
            out.push_back(parkingEndSecond);
            out.push_back(parkingEndMinute);
            out.push_back(parkingEndHour);

            return out;
        }
    };

    struct DeductStopData : CommandDataBase
    {
        uint8_t serialNum[2];
        uint8_t rsv[10];
        uint8_t obuLabel[5];
        uint8_t rsv1[3];

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.insert(out.end(), std::begin(serialNum), std::end(serialNum));
            out.insert(out.end(), std::begin(rsv), std::end(rsv));
            out.insert(out.end(), std::begin(obuLabel), std::end(obuLabel));
            out.insert(out.end(), std::begin(rsv1), std::end(rsv1));

            return out;
        }
    };

    struct TransactionReqData : CommandDataBase
    {
        uint8_t serialNum[2];
        uint8_t rsv[10];
        uint8_t obuLabel[5];
        uint8_t rsv1[3];

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.insert(out.end(), std::begin(serialNum), std::end(serialNum));
            out.insert(out.end(), std::begin(rsv), std::end(rsv));
            out.insert(out.end(), std::begin(obuLabel), std::end(obuLabel));
            out.insert(out.end(), std::begin(rsv1), std::end(rsv1));

            return out;
        }
    };

    struct CPOInfoDisplayData : CommandDataBase
    {
        uint8_t rsv[8];
        uint8_t obuLabel[5];
        uint8_t rsv1[3];
        uint8_t dataType;
        uint8_t rsv2[3];
        uint8_t timeout[4];
        uint8_t dataLenOfStoredData[4];
        char dataString1[21];
        char dataString2[21];
        char dataString3[21];
        char dataString4[21];
        char dataString5[21];
        std::vector<uint8_t> storedData;

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.insert(out.end(), std::begin(rsv), std::end(rsv));
            out.insert(out.end(), std::begin(obuLabel), std::end(obuLabel));
            out.insert(out.end(), std::begin(rsv1), std::end(rsv1));
            out.push_back(dataType);
            out.insert(out.end(), std::begin(rsv2), std::end(rsv2));
            out.insert(out.end(), std::begin(timeout), std::end(timeout));
            out.insert(out.end(), std::begin(dataLenOfStoredData), std::end(dataLenOfStoredData));
            out.insert(out.end(), reinterpret_cast<const uint8_t*>(dataString1), reinterpret_cast<const uint8_t*>(dataString1 + sizeof(dataString1)));
            out.insert(out.end(), reinterpret_cast<const uint8_t*>(dataString2), reinterpret_cast<const uint8_t*>(dataString2 + sizeof(dataString2)));
            out.insert(out.end(), reinterpret_cast<const uint8_t*>(dataString3), reinterpret_cast<const uint8_t*>(dataString3 + sizeof(dataString3)));
            out.insert(out.end(), reinterpret_cast<const uint8_t*>(dataString4), reinterpret_cast<const uint8_t*>(dataString4 + sizeof(dataString4)));
            out.insert(out.end(), reinterpret_cast<const uint8_t*>(dataString5), reinterpret_cast<const uint8_t*>(dataString5 + sizeof(dataString5)));
            out.insert(out.end(), storedData.begin(), storedData.end());

            return out;
        }
    };

    struct CarparkProcessCompleteData : CommandDataBase
    {
        uint8_t rsv[8];
        uint8_t obuLabel[5];
        uint8_t rsv1[3];
        uint8_t processingResult;
        uint8_t rsv2;
        uint8_t amt[2];

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.insert(out.end(), std::begin(rsv), std::end(rsv));
            out.insert(out.end(), std::begin(obuLabel), std::end(obuLabel));
            out.insert(out.end(), std::begin(rsv1), std::end(rsv1));
            out.push_back(processingResult);
            out.push_back(rsv2);
            out.insert(out.end(), std::begin(amt), std::end(amt));

            return out;
        }
    };

    struct DSRCProcessCompleteData : CommandDataBase
    {
        uint8_t rsv[8];
        uint8_t obuLabel[5];
        uint8_t rsv1[3];

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.insert(out.end(), std::begin(rsv), std::end(rsv));
            out.insert(out.end(), std::begin(obuLabel), std::end(obuLabel));
            out.insert(out.end(), std::begin(rsv1), std::end(rsv1));

            return out;
        }
    };

    struct StopReqOfRelatedInfoData : CommandDataBase
    {
        uint8_t rsv[8];
        uint8_t obuLabel[5];
        uint8_t rsv1[3];

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.insert(out.end(), std::begin(rsv), std::end(rsv));
            out.insert(out.end(), std::begin(obuLabel), std::end(obuLabel));
            out.insert(out.end(), std::begin(rsv1), std::end(rsv1));

            return out;
        }
    };

    struct TimeCalibrationData : CommandDataBase
    {
        uint8_t day;
        uint8_t month;
        uint8_t year[2];
        uint8_t rsv;
        uint8_t second;
        uint8_t minute;
        uint8_t hour;

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.push_back(day);
            out.push_back(month);
            out.insert(out.end(), std::begin(year), std::end(year));
            out.push_back(rsv);
            out.push_back(second);
            out.push_back(minute);
            out.push_back(hour);

            return out;
        }
    };

    struct SetParkingAvailabilityData : CommandDataBase
    {
        uint8_t availableLots[2];
        uint8_t totalLots[2];
        uint8_t rsv[12];

        std::vector<uint8_t> serialize() const override
        {
            std::vector<uint8_t> out;

            out.insert(out.end(), std::begin(availableLots), std::end(availableLots));
            out.insert(out.end(), std::begin(totalLots), std::end(totalLots));
            out.insert(out.end(), std::begin(rsv), std::end(rsv));

            return out;
        }
    };

    struct MessageHeader
    {
        uint8_t destinationID_;
        uint8_t sourceID_;
        uint8_t dataTypeCode_;
        uint8_t rsv_;
        uint8_t day_;
        uint8_t month_;
        uint16_t year_;
        uint8_t rsv1_;
        uint8_t second_;
        uint8_t minute_;
        uint8_t hour_;
        uint16_t seqNo_;
        uint16_t dataLen_;

        static constexpr std::size_t HEADER_SIZE = 16;

        bool deserialize(const std::vector<uint8_t>& data)
        {
            if (data.size() < HEADER_SIZE)
            {
                return false;
            }

            destinationID_ = data[0];
            sourceID_ = data[1];
            dataTypeCode_ = data[2];
            rsv_ = data[3];
            day_ = data[4];
            month_ = data[5];
            year_ = (data[7] << 8) | data[6];
            rsv1_ = data[8];
            second_ = data[9];
            minute_ = data[10];
            hour_ = data[11];
            seqNo_ = (data[13] << 8) | data[12];
            dataLen_ = (data[15] << 8) | data[14];
            return true;
        }
    };

    enum class CommandType
    {
        ACK,
        NAK,
        HEALTH_STATUS_REQ_CMD,
        WATCHDOG_REQ_CMD,
        START_REQ_CMD,
        STOP_REQ_CMD,
        DI_REQ_CMD,
        SET_DI_PORT_CONFIG_CMD,
        GET_OBU_INFO_REQ_CMD,
        GET_OBU_INFO_STOP_REQ_CMD,
        DEDUCT_REQ_CMD,
        DEDUCT_STOP_REQ_CMD,
        TRANSACTION_REQ_CMD,
        CPO_INFO_DISPLAY_REQ_CMD,
        CARPARK_PROCESS_COMPLETE_NOTIFICATION_REQ_CMD,
        DSRC_PROCESS_COMPLETE_NOTIFICATION_REQ_CMD,
        STOP_REQ_OF_RELATED_INFO_DISTRIBUTION_CMD,
        DSRC_STATUS_REQ_CMD,
        TIME_CALIBRATION_REQ_CMD,
        SET_CARPARK_AVAIL_REQ_CMD,
        CD_DOWNLOAD_REQ_CMD
    };

    struct Command
    {
        CommandType type;
        int priority;   // Lower number = higher priority
        std::shared_ptr<CommandDataBase> data;

        Command() : type(CommandType::WATCHDOG_REQ_CMD), priority(1) {} // Default values
        Command(CommandType t, int prio, std::shared_ptr<CommandDataBase> d = nullptr)
            : type(t), priority(prio), data(std::move(d)) {}
    };

    struct CompareCommand
    {
        bool operator() (const Command& a, const Command& b) const
        {
            return a.priority > b.priority;
        }
    };

    static EEPClient* getInstance();
    void FnEEPClientInit(const std::string& serverIP, unsigned short serverPort, int eepSourceId, int eepDestinationId, int eepCarparkID);
    void FnSendAck(uint8_t reqDataTypeCode_);
    void FnSendNak(uint8_t reqDataTypeCode_, uint8_t reasonCode_);
    void FnSendHealthStatusReq();
    void FnSendWatchdogReq();
    void FnSendStartReq();
    void FnSendStopReq();
    void FnSendDIReq();
    void FnSendSetDIPortConfigReq(uint16_t periodDebounceDi1_, uint16_t periodDebounceDi2_, uint16_t periodDebounceDi3_, uint16_t periodDebounceDi4_, uint16_t periodDebounceDi5_);
    void FnSendGetOBUInfoReq();
    void FnSendGetOBUInfoStopReq();
    void FnSendDeductReq(const std::string& obuLabel_, const std::string& fee_, const std::string& entryTime_, const std::string& exitTime_);
    void FnSendDeductStopReq(const std::string& obuLabel_);
    void FnSendTransactionReq(const std::string& obuLabel_);
    void FnSendCPOInfoDisplayReq(const std::string& obuLabel_, const std::string& dataType_, const std::string& line1_, const std::string& line2_, const std::string& line3_, const std::string& line4_, const std::string& line5_);
    void FnSendCarparkProcessCompleteNotificationReq(const std::string& obuLabel_, const std::string& processingResult_, const std::string& fee);
    void FnSendDSRCProcessCompleteNotificationReq(const std::string& obuLabel_);
    void FnSendStopReqOfRelatedInfoDistributionReq(const std::string& obuLabel_);
    void FnSendDSRCStatusReq();
    void FnSendTimeCalibrationReq();
    void FnSendSetCarparkAvailabilityReq(const std::string& availLots_, const std::string& totalLots_);
    void FnSendCDDownloadReq();
    void FnEEPClientClose();

    /**
     * Singleton EEPClient should not be cloneable
     */
    EEPClient(EEPClient& eep) = delete;

    /**
     * Singleton EEPClient should not be assignable
     */
    void operator=(const EEPClient&) = delete;

private:
    static EEPClient* eepClient_;
    static std::mutex mutex_;
    boost::asio::io_context ioContext_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> workGuard_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::asio::deadline_timer reconnectTimer_;
    boost::asio::deadline_timer connectTimer_;
    boost::asio::deadline_timer sendTimer_;
    boost::asio::deadline_timer responseTimer_;
    boost::asio::deadline_timer watchdogTimer_;
    boost::asio::deadline_timer healthStatusTimer_;
    std::unique_ptr<AppTcpClient> client_;
    std::string logFileName_;
    std::thread ioContextThread_;
    static const StateTransition stateTransitionTable[static_cast<int>(STATE::STATE_COUNT)];
    STATE currentState_;
    std::mutex cmdQueueMutex_;
    static std::mutex currentCmdMutex_;
    std::priority_queue<Command, std::vector<Command>, CompareCommand> commandQueue_;
    Command currentCmd;
    static uint16_t sequenceNo_;
    static std::mutex sequenceNoMutex_;
    std::string serverIP_;
    unsigned short serverPort_;
    int eepSourceId_;
    int eepDestinationId_;
    int eepCarparkID_;
    static uint16_t deductCmdSerialNo_;
    static uint16_t lastDeductCmdSerialNo_;
    static std::mutex deductCmdSerialNoMutex_;
    int watchdogMissedRspCount_;
    EEPClient();
    void startIoContextThread();
    void handleConnect(bool success, const std::string& message);
    void handleSend(bool success, const std::string& message);
    void handleClose(bool success, const std::string& message);
    void handleReceivedData(bool success, const std::vector<uint8_t>& data);
    void eepClientConnect();
    void eepClientSend(const std::vector<uint8_t>& message);
    void eepClientClose();
    std::string messageCodeToString(MESSAGE_CODE code);
    std::string eventToString(EVENT event);
    std::string stateToString(STATE state);
    void processEvent(EVENT event);
    void checkCommandQueue();
    void enqueueCommand(CommandType type, int priority, std::shared_ptr<CommandDataBase> data);
    void popFromCommandQueueAndEnqueueWrite();
    std::string getCommandString(CommandType cmd);
    void setCurrentCmd(Command cmd);
    Command getCurrentCmd();
    void incrementSequenceNo();
    uint16_t getSequenceNo();
    void incrementDeductCmdSerialNo();
    uint16_t getLastDeductCmdSerialNo();
    uint16_t getDeductCmdSerialNo();
    void appendMessageHeader(std::vector<uint8_t>& msg, uint8_t messageCode, uint16_t seqNo, uint16_t length);
    uint32_t calculateChecksumNoPadding(const std::vector<uint8_t>& data);
    std::vector<uint8_t> prepareCmd(Command cmd);
    void startReconnectTimer();
    void startConnectTimer();
    void handleConnectTimerTimeout(const boost::system::error_code& error);
    void startSendTimer();
    void handleSendTimerTimeout(const boost::system::error_code& error);
    void startResponseTimer();
    void handleResponseTimeout(const boost::system::error_code& error);
    void startWatchdogTimer();
    void handleWatchdogTimeout(const boost::system::error_code& error);
    void startHealthStatusTimer();
    void handleHealthStatusTimeout(const boost::system::error_code& error);
    void handleIdleState(EVENT event);
    void handleConnectingState(EVENT event);
    void handleConnectedState(EVENT event);
    void handleWritingRequestState(EVENT event);
    void handleWaitingForResponseState(EVENT event);
    void handleCommandErrorOrTimeout(Command cmd, MSG_STATUS msgStatus);
    bool isValidCheckSum(const std::vector<uint8_t>& data);
    bool parseMessage(const std::vector<uint8_t>& data, MessageHeader& header, std::vector<uint8_t>& body);
    void printField(std::ostringstream& oss, const std::string& label, uint64_t value, int hexWidth, const std::string& remark);
    void printFieldChar(std::ostringstream& oss, const std::string& label, const std::vector<uint8_t>& data, const std::string& remark);
    bool isValidSourceDestination(uint8_t source, uint8_t destination);
    std::string getFieldDescription(uint8_t value, const std::unordered_map<uint8_t, std::string>& map);
    void showParsedMessage(const MessageHeader& header, const std::vector<uint8_t>& body);
    void handleParsedMessage(const MessageHeader& header, const std::vector<uint8_t>& body);
};