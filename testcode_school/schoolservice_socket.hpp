#ifndef SCHOOLSERVICE_SOCKET_HPP
#define SCHOOLSERVICE_SOCKET_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <queue>
#include <algorithm>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

namespace ipc {

#ifndef IPC_BYTE_BUFFER_DEFINED
#define IPC_BYTE_BUFFER_DEFINED
// Serialization helpers
class ByteBuffer {
private:
    std::vector<uint8_t> data_;

public:
    void writeUint32(uint32_t value) {
        data_.push_back((value >> 24) & 0xFF);
        data_.push_back((value >> 16) & 0xFF);
        data_.push_back((value >> 8) & 0xFF);
        data_.push_back(value & 0xFF);
    }

    void writeInt32(int32_t value) {
        writeUint32(static_cast<uint32_t>(value));
    }
    
    void writeUint64(uint64_t value) {
        writeUint32(static_cast<uint32_t>(value >> 32));
        writeUint32(static_cast<uint32_t>(value & 0xFFFFFFFF));
    }
    
    void writeInt64(int64_t value) {
        writeUint64(static_cast<uint64_t>(value));
    }
    
    void writeUint16(uint16_t value) {
        data_.push_back((value >> 8) & 0xFF);
        data_.push_back(value & 0xFF);
    }
    
    void writeInt16(int16_t value) {
        writeUint16(static_cast<uint16_t>(value));
    }
    
    void writeUint8(uint8_t value) {
        data_.push_back(value);
    }
    
    void writeInt8(int8_t value) {
        data_.push_back(static_cast<uint8_t>(value));
    }
    
    void writeChar(char value) {
        data_.push_back(static_cast<uint8_t>(value));
    }

    void writeBool(bool value) {
        data_.push_back(value ? 1 : 0);
    }
    
    void writeDouble(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        writeUint32(static_cast<uint32_t>(bits >> 32));
        writeUint32(static_cast<uint32_t>(bits & 0xFFFFFFFF));
    }
    
    void writeFloat(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        writeUint32(bits);
    }

    void writeString(const std::string& str) {
        writeUint32(str.size());
        data_.insert(data_.end(), str.begin(), str.end());
    }

    void writeStringVector(const std::vector<std::string>& vec) {
        writeUint32(vec.size());
        for (const auto& item : vec) {
            writeString(item);
        }
    }

    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    void clear() { data_.clear(); }
};

class ByteReader {
private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_;

public:
    ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size), pos_(0) {}

    bool canRead(size_t bytes) const {
        return pos_ + bytes <= size_;
    }

    uint32_t readUint32() {
        if (!canRead(4)) throw std::runtime_error("Buffer underflow");
        uint32_t value = (static_cast<uint32_t>(data_[pos_]) << 24) | 
                         (static_cast<uint32_t>(data_[pos_+1]) << 16) |
                         (static_cast<uint32_t>(data_[pos_+2]) << 8) | 
                         static_cast<uint32_t>(data_[pos_+3]);
        pos_ += 4;
        return value;
    }

    int32_t readInt32() {
        return static_cast<int32_t>(readUint32());
    }
    
    uint64_t readUint64() {
        uint32_t high = readUint32();
        uint32_t low = readUint32();
        return (static_cast<uint64_t>(high) << 32) | low;
    }
    
    int64_t readInt64() {
        return static_cast<int64_t>(readUint64());
    }
    
    uint16_t readUint16() {
        if (!canRead(2)) throw std::runtime_error("Buffer underflow");
        uint16_t value = (static_cast<uint16_t>(data_[pos_]) << 8) | 
                         static_cast<uint16_t>(data_[pos_+1]);
        pos_ += 2;
        return value;
    }
    
    int16_t readInt16() {
        return static_cast<int16_t>(readUint16());
    }
    
    uint8_t readUint8() {
        if (!canRead(1)) throw std::runtime_error("Buffer underflow");
        return data_[pos_++];
    }
    
    int8_t readInt8() {
        return static_cast<int8_t>(readUint8());
    }
    
    char readChar() {
        if (!canRead(1)) throw std::runtime_error("Buffer underflow");
        return static_cast<char>(data_[pos_++]);
    }

    bool readBool() {
        if (!canRead(1)) throw std::runtime_error("Buffer underflow");
        return data_[pos_++] != 0;
    }
    
    double readDouble() {
        uint64_t bits = (static_cast<uint64_t>(readUint32()) << 32) | readUint32();
        double value;
        std::memcpy(&value, &bits, sizeof(double));
        return value;
    }
    
    float readFloat() {
        uint32_t bits = readUint32();
        float value;
        std::memcpy(&value, &bits, sizeof(float));
        return value;
    }

    std::string readString() {
        uint32_t len = readUint32();
        if (!canRead(len)) throw std::runtime_error("Buffer underflow");
        std::string str(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return str;
    }

    std::vector<std::string> readStringVector() {
        uint32_t count = readUint32();
        std::vector<std::string> vec;
        vec.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            vec.push_back(readString());
        }
        return vec;
    }

    size_t position() const { return pos_; }
};
#endif // IPC_BYTE_BUFFER_DEFINED

// Message IDs
const uint32_t MSG_ADDSTUDENT_REQ = 1000;
const uint32_t MSG_ADDSTUDENT_RESP = 1001;
const uint32_t MSG_ADDTEACHER_REQ = 1002;
const uint32_t MSG_ADDTEACHER_RESP = 1003;
const uint32_t MSG_GETPERSONINFO_REQ = 1004;
const uint32_t MSG_GETPERSONINFO_RESP = 1005;
const uint32_t MSG_UPDATEPERSONINFO_REQ = 1006;
const uint32_t MSG_UPDATEPERSONINFO_RESP = 1007;
const uint32_t MSG_REMOVEPERSON_REQ = 1008;
const uint32_t MSG_REMOVEPERSON_RESP = 1009;
const uint32_t MSG_BATCHADDSTUDENTS_REQ = 1010;
const uint32_t MSG_BATCHADDSTUDENTS_RESP = 1011;
const uint32_t MSG_BATCHQUERYPERSONS_REQ = 1012;
const uint32_t MSG_BATCHQUERYPERSONS_RESP = 1013;
const uint32_t MSG_ADDCOURSE_REQ = 1014;
const uint32_t MSG_ADDCOURSE_RESP = 1015;
const uint32_t MSG_GETALLCOURSES_REQ = 1016;
const uint32_t MSG_GETALLCOURSES_RESP = 1017;
const uint32_t MSG_ENROLLCOURSE_REQ = 1018;
const uint32_t MSG_ENROLLCOURSE_RESP = 1019;
const uint32_t MSG_DROPCOURSE_REQ = 1020;
const uint32_t MSG_DROPCOURSE_RESP = 1021;
const uint32_t MSG_SUBMITGRADE_REQ = 1022;
const uint32_t MSG_SUBMITGRADE_RESP = 1023;
const uint32_t MSG_GETSTUDENTGRADES_REQ = 1024;
const uint32_t MSG_GETSTUDENTGRADES_RESP = 1025;
const uint32_t MSG_BATCHSUBMITGRADES_REQ = 1026;
const uint32_t MSG_BATCHSUBMITGRADES_RESP = 1027;
const uint32_t MSG_QUERYBYTYPE_REQ = 1028;
const uint32_t MSG_QUERYBYTYPE_RESP = 1029;
const uint32_t MSG_GETSTATISTICS_REQ = 1030;
const uint32_t MSG_GETSTATISTICS_RESP = 1031;
const uint32_t MSG_SEARCHPERSONS_REQ = 1032;
const uint32_t MSG_SEARCHPERSONS_RESP = 1033;
const uint32_t MSG_GETTOTALCOUNT_REQ = 1034;
const uint32_t MSG_GETTOTALCOUNT_RESP = 1035;
const uint32_t MSG_CLEARALL_REQ = 1036;
const uint32_t MSG_ONPERSONCHANGED_REQ = 1037;
const uint32_t MSG_ONBATCHEVENTS_REQ = 1038;
const uint32_t MSG_ONSYSTEMSTATUS_REQ = 1039;
const uint32_t MSG_ONSTATISTICSUPDATED_REQ = 1040;

#ifndef IPC_SCHOOLMANAGEMENT_TYPES_DEFINED
#define IPC_SCHOOLMANAGEMENT_TYPES_DEFINED
enum class PersonType {
    STUDENT,
    TEACHER,
    STAFF,
    ADMIN
};

enum class Gender {
    MALE,
    FEMALE,
    OTHER
};

enum class OperationStatus {
    SUCCESS,
    NOT_FOUND,
    ALREADY_EXISTS,
    INVALID_DATA,
    PERMISSION_DENIED,
    ERROR
};

enum class EventType {
    PERSON_ADDED,
    PERSON_UPDATED,
    PERSON_REMOVED,
    COURSE_ENROLLED,
    COURSE_DROPPED,
    GRADE_UPDATED
};

struct Address {
    std::string street;
    std::string city;
    std::string province;
    std::string postalCode;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeString(street);
        buffer.writeString(city);
        buffer.writeString(province);
        buffer.writeString(postalCode);
    }

    void deserialize(ByteReader& reader) {
        street = reader.readString();
        city = reader.readString();
        province = reader.readString();
        postalCode = reader.readString();
    }
};

struct Course {
    std::string courseId;
    std::string courseName;
    std::string teacherId;
    int64_t credits;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeString(courseId);
        buffer.writeString(courseName);
        buffer.writeString(teacherId);
        buffer.writeInt64(credits);
    }

    void deserialize(ByteReader& reader) {
        courseId = reader.readString();
        courseName = reader.readString();
        teacherId = reader.readString();
        credits = reader.readInt64();
    }
};

struct Grade {
    std::string studentId;
    std::string courseId;
    int64_t score;
    int64_t timestamp;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeString(studentId);
        buffer.writeString(courseId);
        buffer.writeInt64(score);
        buffer.writeInt64(timestamp);
    }

    void deserialize(ByteReader& reader) {
        studentId = reader.readString();
        courseId = reader.readString();
        score = reader.readInt64();
        timestamp = reader.readInt64();
    }
};

struct PersonInfo {
    std::string personId;
    std::string name;
    int64_t age;
    Gender gender;
    PersonType personType;
    std::string email;
    std::string phone;
    Address address;
    int64_t createTime;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeString(personId);
        buffer.writeString(name);
        buffer.writeInt64(age);
        buffer.writeInt32(static_cast<int32_t>(gender));
        buffer.writeInt32(static_cast<int32_t>(personType));
        buffer.writeString(email);
        buffer.writeString(phone);
        address.serialize(buffer);
        buffer.writeInt64(createTime);
    }

    void deserialize(ByteReader& reader) {
        personId = reader.readString();
        name = reader.readString();
        age = reader.readInt64();
        gender = static_cast<Gender>(reader.readInt32());
        personType = static_cast<PersonType>(reader.readInt32());
        email = reader.readString();
        phone = reader.readString();
        address.deserialize(reader);
        createTime = reader.readInt64();
    }
};

struct StudentDetails {
    PersonInfo basicInfo;
    std::string major;
    int64_t enrollmentYear;
    double gpa;

    void serialize(ByteBuffer& buffer) const {
        basicInfo.serialize(buffer);
        buffer.writeString(major);
        buffer.writeInt64(enrollmentYear);
        buffer.writeDouble(gpa);
    }

    void deserialize(ByteReader& reader) {
        basicInfo.deserialize(reader);
        major = reader.readString();
        enrollmentYear = reader.readInt64();
        gpa = reader.readDouble();
    }
};

struct TeacherDetails {
    PersonInfo basicInfo;
    std::string department;
    std::string title;
    int64_t yearsOfService;

    void serialize(ByteBuffer& buffer) const {
        basicInfo.serialize(buffer);
        buffer.writeString(department);
        buffer.writeString(title);
        buffer.writeInt64(yearsOfService);
    }

    void deserialize(ByteReader& reader) {
        basicInfo.deserialize(reader);
        department = reader.readString();
        title = reader.readString();
        yearsOfService = reader.readInt64();
    }
};

struct NotificationEvent {
    EventType eventType;
    std::string personId;
    std::string description;
    int64_t timestamp;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeInt32(static_cast<int32_t>(eventType));
        buffer.writeString(personId);
        buffer.writeString(description);
        buffer.writeInt64(timestamp);
    }

    void deserialize(ByteReader& reader) {
        eventType = static_cast<EventType>(reader.readInt32());
        personId = reader.readString();
        description = reader.readString();
        timestamp = reader.readInt64();
    }
};

struct Statistics {
    int64_t totalStudents;
    int64_t totalTeachers;
    int64_t totalStaff;
    int64_t totalCourses;
    double averageGPA;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeInt64(totalStudents);
        buffer.writeInt64(totalTeachers);
        buffer.writeInt64(totalStaff);
        buffer.writeInt64(totalCourses);
        buffer.writeDouble(averageGPA);
    }

    void deserialize(ByteReader& reader) {
        totalStudents = reader.readInt64();
        totalTeachers = reader.readInt64();
        totalStaff = reader.readInt64();
        totalCourses = reader.readInt64();
        averageGPA = reader.readDouble();
    }
};

#endif // IPC_SCHOOLMANAGEMENT_TYPES_DEFINED

// Message Structures
struct addStudentRequest {
    uint32_t msg_id = MSG_ADDSTUDENT_REQ;
    StudentDetails student;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        student.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        student.deserialize(reader);
    }
};

struct addStudentResponse {
    uint32_t msg_id = MSG_ADDSTUDENT_RESP;
    int32_t status = 0;
    OperationStatus return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt32(static_cast<int32_t>(return_value));
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = static_cast<OperationStatus>(reader.readInt32());
    }
};

struct addTeacherRequest {
    uint32_t msg_id = MSG_ADDTEACHER_REQ;
    TeacherDetails teacher;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        teacher.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        teacher.deserialize(reader);
    }
};

struct addTeacherResponse {
    uint32_t msg_id = MSG_ADDTEACHER_RESP;
    int32_t status = 0;
    OperationStatus return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt32(static_cast<int32_t>(return_value));
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = static_cast<OperationStatus>(reader.readInt32());
    }
};

struct getPersonInfoRequest {
    uint32_t msg_id = MSG_GETPERSONINFO_REQ;
    std::string personId;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(personId);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        personId = reader.readString();
    }
};

struct getPersonInfoResponse {
    uint32_t msg_id = MSG_GETPERSONINFO_RESP;
    int32_t status = 0;
    PersonInfo return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        return_value.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value.deserialize(reader);
    }
};

struct updatePersonInfoRequest {
    uint32_t msg_id = MSG_UPDATEPERSONINFO_REQ;
    std::string personId;
    PersonInfo info;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(personId);
        info.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        personId = reader.readString();
        info.deserialize(reader);
    }
};

struct updatePersonInfoResponse {
    uint32_t msg_id = MSG_UPDATEPERSONINFO_RESP;
    int32_t status = 0;
    bool return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeBool(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readBool();
    }
};

struct removePersonRequest {
    uint32_t msg_id = MSG_REMOVEPERSON_REQ;
    std::string personId;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(personId);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        personId = reader.readString();
    }
};

struct removePersonResponse {
    uint32_t msg_id = MSG_REMOVEPERSON_RESP;
    int32_t status = 0;
    bool return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeBool(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readBool();
    }
};

struct batchAddStudentsRequest {
    uint32_t msg_id = MSG_BATCHADDSTUDENTS_REQ;
    std::vector<StudentDetails> students;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(students.size());
        for (const auto& item : students) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            students.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                students[i].deserialize(reader);
            }
        }
    }
};

struct batchAddStudentsResponse {
    uint32_t msg_id = MSG_BATCHADDSTUDENTS_RESP;
    int32_t status = 0;
    int64_t return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt64(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readInt64();
    }
};

struct batchQueryPersonsRequest {
    uint32_t msg_id = MSG_BATCHQUERYPERSONS_REQ;
    std::vector<std::string> personIds;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeStringVector(personIds);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        personIds = reader.readStringVector();
    }
};

struct batchQueryPersonsResponse {
    uint32_t msg_id = MSG_BATCHQUERYPERSONS_RESP;
    std::vector<PersonInfo> infos;
    std::vector<OperationStatus> status;
    int32_t response_status = 0;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(infos.size());
        for (const auto& item : infos) {
            item.serialize(buffer);
        }
        buffer.writeUint32(status.size());
        for (const auto& item : status) {
            buffer.writeInt32(static_cast<int32_t>(item));
        }
        buffer.writeInt32(response_status);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            infos.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                infos[i].deserialize(reader);
            }
        }
        {
            uint32_t count = reader.readUint32();
            status.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                status[i] = static_cast<OperationStatus>(reader.readInt32());
            }
        }
        response_status = reader.readInt32();
    }
};

struct addCourseRequest {
    uint32_t msg_id = MSG_ADDCOURSE_REQ;
    Course course;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        course.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        course.deserialize(reader);
    }
};

struct addCourseResponse {
    uint32_t msg_id = MSG_ADDCOURSE_RESP;
    int32_t status = 0;
    OperationStatus return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt32(static_cast<int32_t>(return_value));
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = static_cast<OperationStatus>(reader.readInt32());
    }
};

struct getAllCoursesRequest {
    uint32_t msg_id = MSG_GETALLCOURSES_REQ;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
    }
};

struct getAllCoursesResponse {
    uint32_t msg_id = MSG_GETALLCOURSES_RESP;
    int32_t status = 0;
    std::vector<Course> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i].deserialize(reader);
            }
        }
    }
};

struct enrollCourseRequest {
    uint32_t msg_id = MSG_ENROLLCOURSE_REQ;
    std::string studentId;
    std::string courseId;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(studentId);
        buffer.writeString(courseId);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        studentId = reader.readString();
        courseId = reader.readString();
    }
};

struct enrollCourseResponse {
    uint32_t msg_id = MSG_ENROLLCOURSE_RESP;
    int32_t status = 0;
    bool return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeBool(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readBool();
    }
};

struct dropCourseRequest {
    uint32_t msg_id = MSG_DROPCOURSE_REQ;
    std::string studentId;
    std::string courseId;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(studentId);
        buffer.writeString(courseId);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        studentId = reader.readString();
        courseId = reader.readString();
    }
};

struct dropCourseResponse {
    uint32_t msg_id = MSG_DROPCOURSE_RESP;
    int32_t status = 0;
    bool return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeBool(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readBool();
    }
};

struct submitGradeRequest {
    uint32_t msg_id = MSG_SUBMITGRADE_REQ;
    Grade grade;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        grade.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        grade.deserialize(reader);
    }
};

struct submitGradeResponse {
    uint32_t msg_id = MSG_SUBMITGRADE_RESP;
    int32_t status = 0;
    bool return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeBool(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readBool();
    }
};

struct getStudentGradesRequest {
    uint32_t msg_id = MSG_GETSTUDENTGRADES_REQ;
    std::string studentId;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(studentId);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        studentId = reader.readString();
    }
};

struct getStudentGradesResponse {
    uint32_t msg_id = MSG_GETSTUDENTGRADES_RESP;
    int32_t status = 0;
    std::vector<Grade> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i].deserialize(reader);
            }
        }
    }
};

struct batchSubmitGradesRequest {
    uint32_t msg_id = MSG_BATCHSUBMITGRADES_REQ;
    std::vector<Grade> grades;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(grades.size());
        for (const auto& item : grades) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            grades.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                grades[i].deserialize(reader);
            }
        }
    }
};

struct batchSubmitGradesResponse {
    uint32_t msg_id = MSG_BATCHSUBMITGRADES_RESP;
    int32_t status = 0;
    int64_t return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt64(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readInt64();
    }
};

struct queryByTypeRequest {
    uint32_t msg_id = MSG_QUERYBYTYPE_REQ;
    PersonType personType;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(static_cast<int32_t>(personType));
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        personType = static_cast<PersonType>(reader.readInt32());
    }
};

struct queryByTypeResponse {
    uint32_t msg_id = MSG_QUERYBYTYPE_RESP;
    int32_t status = 0;
    std::vector<PersonInfo> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i].deserialize(reader);
            }
        }
    }
};

struct getStatisticsRequest {
    uint32_t msg_id = MSG_GETSTATISTICS_REQ;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
    }
};

struct getStatisticsResponse {
    uint32_t msg_id = MSG_GETSTATISTICS_RESP;
    int32_t status = 0;
    Statistics return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        return_value.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value.deserialize(reader);
    }
};

struct searchPersonsRequest {
    uint32_t msg_id = MSG_SEARCHPERSONS_REQ;
    std::string keyword;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(keyword);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        keyword = reader.readString();
    }
};

struct searchPersonsResponse {
    uint32_t msg_id = MSG_SEARCHPERSONS_RESP;
    int32_t status = 0;
    std::vector<PersonInfo> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i].deserialize(reader);
            }
        }
    }
};

struct getTotalCountRequest {
    uint32_t msg_id = MSG_GETTOTALCOUNT_REQ;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
    }
};

struct getTotalCountResponse {
    uint32_t msg_id = MSG_GETTOTALCOUNT_RESP;
    int32_t status = 0;
    int64_t return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt64(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readInt64();
    }
};

struct clearAllRequest {
    uint32_t msg_id = MSG_CLEARALL_REQ;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
    }
};


struct onPersonChangedRequest {
    uint32_t msg_id = MSG_ONPERSONCHANGED_REQ;
    NotificationEvent event;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        event.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        event.deserialize(reader);
    }
};


struct onBatchEventsRequest {
    uint32_t msg_id = MSG_ONBATCHEVENTS_REQ;
    std::vector<NotificationEvent> events;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(events.size());
        for (const auto& item : events) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            events.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                events[i].deserialize(reader);
            }
        }
    }
};


struct onSystemStatusRequest {
    uint32_t msg_id = MSG_ONSYSTEMSTATUS_REQ;
    bool isOnline;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeBool(isOnline);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        isOnline = reader.readBool();
    }
};


struct onStatisticsUpdatedRequest {
    uint32_t msg_id = MSG_ONSTATISTICSUPDATED_REQ;
    Statistics stats;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        stats.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        stats.deserialize(reader);
    }
};


#ifndef IPC_SOCKET_BASE_DEFINED
#define IPC_SOCKET_BASE_DEFINED
// Socket Base Class
class SocketBase {
protected:
    int sockfd_;
    struct sockaddr_in addr_;
    bool connected_;
    
public:
    SocketBase() : sockfd_(-1), connected_(false) {}
    
    virtual ~SocketBase() {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
    }
    
    bool isConnected() const { return connected_; }
    
    ssize_t sendData(const void* data, size_t size) {
        return send(sockfd_, data, size, 0);
    }
    
    ssize_t receiveData(void* buffer, size_t size) {
        return recv(sockfd_, buffer, size, 0);
    }
    
    static ssize_t sendDataToSocket(int fd, const void* data, size_t size) {
        return send(fd, data, size, 0);
    }
    
    static ssize_t receiveDataFromSocket(int fd, void* buffer, size_t size) {
        return recv(fd, buffer, size, 0);
    }
};
#endif // IPC_SOCKET_BASE_DEFINED

// Client Interface for SchoolService
class SchoolServiceClient : public SocketBase {
private:
    std::thread listener_thread_;
    bool listening_;
    std::mutex send_mutex_;

    // Message queue for RPC responses
    struct QueuedMessage {
        uint32_t msg_id;
        std::vector<uint8_t> data;
    };
    std::queue<QueuedMessage> rpc_response_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

public:
    SchoolServiceClient() : listening_(false) {}

    ~SchoolServiceClient() {
        stopListening();
    }

    // Connect to server
    bool connect(const std::string& host, uint16_t port) {
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            return false;
        }

        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr_.sin_addr);

        if (::connect(sockfd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        connected_ = true;
        
        // Auto-start listener thread for message reception
        startListening();
        
        return true;
    }

    // Start async listening for broadcast messages
    void startListening() {
        if (listening_ || !connected_) return;
        listening_ = true;
        listener_thread_ = std::thread([this]() {
            listenLoop();
        });
    }

    // Stop async listening
    void stopListening() {
        listening_ = false;
        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }
    }

private:
    void listenLoop() {
        while (listening_ && connected_) {
            // Set recv timeout to allow periodic checking of listening_ flag
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            // Receive message size
            uint32_t msg_size;
            ssize_t received = recv(sockfd_, &msg_size, sizeof(msg_size), 0);

            if (received <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue; // Timeout, continue listening
                }
                break; // Connection closed or error
            }

            // Receive message data
            uint8_t recv_buffer[65536];
            ssize_t recv_size = recv(sockfd_, recv_buffer, msg_size, 0);
            if (recv_size < 0 || recv_size != msg_size) {
                break;
            }

            // Parse message ID
            if (msg_size < 4) continue;
            uint32_t msg_id = (static_cast<uint32_t>(recv_buffer[0]) << 24) |
                              (static_cast<uint32_t>(recv_buffer[1]) << 16) |
                              (static_cast<uint32_t>(recv_buffer[2]) << 8) |
                              static_cast<uint32_t>(recv_buffer[3]);

            // Check if this is a callback message (REQ) or RPC response (RESP)
            bool is_callback = isCallbackMessage(msg_id);

            if (is_callback) {
                // Handle callback directly
                handleBroadcastMessage(msg_id, recv_buffer, recv_size);
            } else {
                // Queue RPC response for RPC method to retrieve
                QueuedMessage msg;
                msg.msg_id = msg_id;
                msg.data.assign(recv_buffer, recv_buffer + recv_size);
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    rpc_response_queue_.push(msg);
                }
                queue_cv_.notify_one();
            }
        }
    }

    bool isCallbackMessage(uint32_t msg_id) {
        // Check if message ID corresponds to a callback (REQ message)
        switch (msg_id) {
            case MSG_ONPERSONCHANGED_REQ:
            case MSG_ONBATCHEVENTS_REQ:
            case MSG_ONSYSTEMSTATUS_REQ:
            case MSG_ONSTATISTICSUPDATED_REQ:
                return true;
            default:
                return false;
        }
    }

    void handleBroadcastMessage(uint32_t msg_id, const uint8_t* data, size_t size) {
        ByteReader reader(data, size);
        
        switch (msg_id) {
            case MSG_ONPERSONCHANGED_REQ: {
                onPersonChangedRequest request;
                request.deserialize(reader);
                onPersonChanged(request.event);
                break;
            }
            case MSG_ONBATCHEVENTS_REQ: {
                onBatchEventsRequest request;
                request.deserialize(reader);
                onBatchEvents(request.events);
                break;
            }
            case MSG_ONSYSTEMSTATUS_REQ: {
                onSystemStatusRequest request;
                request.deserialize(reader);
                onSystemStatus(request.isOnline);
                break;
            }
            case MSG_ONSTATISTICSUPDATED_REQ: {
                onStatisticsUpdatedRequest request;
                request.deserialize(reader);
                onStatisticsUpdated(request.stats);
                break;
            }
            default:
                std::cout << "[Client] Received unknown broadcast message: " << msg_id << std::endl;
                break;
        }
    }

protected:
    // Callback methods (marked with 'callback' keyword in IDL)
    virtual void onPersonChanged(NotificationEvent event) {
        // Override to handle onPersonChanged callback from server
        std::cout << "[Client] 📢 Callback: onPersonChanged" << std::endl;
    }

    virtual void onBatchEvents(std::vector<NotificationEvent> events) {
        // Override to handle onBatchEvents callback from server
        std::cout << "[Client] 📢 Callback: onBatchEvents" << std::endl;
    }

    virtual void onSystemStatus(bool isOnline) {
        // Override to handle onSystemStatus callback from server
        std::cout << "[Client] 📢 Callback: onSystemStatus" << std::endl;
    }

    virtual void onStatisticsUpdated(Statistics stats) {
        // Override to handle onStatisticsUpdated callback from server
        std::cout << "[Client] 📢 Callback: onStatisticsUpdated" << std::endl;
    }

public:

    OperationStatus addStudent(StudentDetails student) {
        if (!connected_) {
            return OperationStatus();
        }

        // Prepare request
        addStudentRequest request;
        request.student = student;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return OperationStatus();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return OperationStatus();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_ADDSTUDENT_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return OperationStatus(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        addStudentResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    OperationStatus addTeacher(TeacherDetails teacher) {
        if (!connected_) {
            return OperationStatus();
        }

        // Prepare request
        addTeacherRequest request;
        request.teacher = teacher;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return OperationStatus();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return OperationStatus();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_ADDTEACHER_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return OperationStatus(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        addTeacherResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    PersonInfo getPersonInfo(const std::string& personId) {
        if (!connected_) {
            return PersonInfo();
        }

        // Prepare request
        getPersonInfoRequest request;
        request.personId = personId;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return PersonInfo();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return PersonInfo();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_GETPERSONINFO_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return PersonInfo(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        getPersonInfoResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool updatePersonInfo(const std::string& personId, PersonInfo info) {
        if (!connected_) {
            return bool();
        }

        // Prepare request
        updatePersonInfoRequest request;
        request.personId = personId;
        request.info = info;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return bool();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return bool();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_UPDATEPERSONINFO_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return bool(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        updatePersonInfoResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool removePerson(const std::string& personId) {
        if (!connected_) {
            return bool();
        }

        // Prepare request
        removePersonRequest request;
        request.personId = personId;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return bool();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return bool();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_REMOVEPERSON_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return bool(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        removePersonResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    int64_t batchAddStudents(std::vector<StudentDetails> students) {
        if (!connected_) {
            return int64_t();
        }

        // Prepare request
        batchAddStudentsRequest request;
        request.students = students;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return int64_t();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return int64_t();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_BATCHADDSTUDENTS_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return int64_t(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        batchAddStudentsResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool batchQueryPersons(std::vector<std::string> personIds, std::vector<PersonInfo>& infos, std::vector<OperationStatus>& status) {
        if (!connected_) {
            return false;
        }

        // Prepare request
        batchQueryPersonsRequest request;
        request.personIds = personIds;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return false;
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return false;
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_BATCHQUERYPERSONS_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return false; // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        batchQueryPersonsResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        infos = response.infos;
        status = response.status;
        return response.response_status == 0;
    }

    OperationStatus addCourse(Course course) {
        if (!connected_) {
            return OperationStatus();
        }

        // Prepare request
        addCourseRequest request;
        request.course = course;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return OperationStatus();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return OperationStatus();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_ADDCOURSE_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return OperationStatus(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        addCourseResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<Course> getAllCourses() {
        if (!connected_) {
            return std::vector<Course>();
        }

        // Prepare request
        getAllCoursesRequest request;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<Course>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<Course>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_GETALLCOURSES_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return std::vector<Course>(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        getAllCoursesResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool enrollCourse(const std::string& studentId, const std::string& courseId) {
        if (!connected_) {
            return bool();
        }

        // Prepare request
        enrollCourseRequest request;
        request.studentId = studentId;
        request.courseId = courseId;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return bool();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return bool();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_ENROLLCOURSE_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return bool(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        enrollCourseResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool dropCourse(const std::string& studentId, const std::string& courseId) {
        if (!connected_) {
            return bool();
        }

        // Prepare request
        dropCourseRequest request;
        request.studentId = studentId;
        request.courseId = courseId;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return bool();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return bool();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_DROPCOURSE_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return bool(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        dropCourseResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool submitGrade(Grade grade) {
        if (!connected_) {
            return bool();
        }

        // Prepare request
        submitGradeRequest request;
        request.grade = grade;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return bool();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return bool();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_SUBMITGRADE_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return bool(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        submitGradeResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<Grade> getStudentGrades(const std::string& studentId) {
        if (!connected_) {
            return std::vector<Grade>();
        }

        // Prepare request
        getStudentGradesRequest request;
        request.studentId = studentId;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<Grade>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<Grade>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_GETSTUDENTGRADES_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return std::vector<Grade>(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        getStudentGradesResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    int64_t batchSubmitGrades(std::vector<Grade> grades) {
        if (!connected_) {
            return int64_t();
        }

        // Prepare request
        batchSubmitGradesRequest request;
        request.grades = grades;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return int64_t();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return int64_t();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_BATCHSUBMITGRADES_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return int64_t(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        batchSubmitGradesResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<PersonInfo> queryByType(PersonType personType) {
        if (!connected_) {
            return std::vector<PersonInfo>();
        }

        // Prepare request
        queryByTypeRequest request;
        request.personType = personType;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<PersonInfo>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<PersonInfo>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_QUERYBYTYPE_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return std::vector<PersonInfo>(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        queryByTypeResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    Statistics getStatistics() {
        if (!connected_) {
            return Statistics();
        }

        // Prepare request
        getStatisticsRequest request;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return Statistics();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return Statistics();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_GETSTATISTICS_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return Statistics(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        getStatisticsResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<PersonInfo> searchPersons(const std::string& keyword) {
        if (!connected_) {
            return std::vector<PersonInfo>();
        }

        // Prepare request
        searchPersonsRequest request;
        request.keyword = keyword;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<PersonInfo>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<PersonInfo>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_SEARCHPERSONS_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return std::vector<PersonInfo>(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        searchPersonsResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    int64_t getTotalCount() {
        if (!connected_) {
            return int64_t();
        }

        // Prepare request
        getTotalCountRequest request;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return int64_t();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return int64_t();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_GETTOTALCOUNT_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return int64_t(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        getTotalCountResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool clearAll() {
        if (!connected_) {
            return false;
        }

        // Prepare request
        clearAllRequest request;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return false;
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return false;
        }

        return true;
    }

};

// Server Interface for SchoolService
class SchoolServiceServer : public SocketBase {
private:
    int listen_fd_;
    bool running_;
    std::vector<int> client_fds_;
    mutable std::mutex clients_mutex_;
    std::vector<std::thread> client_threads_;

public:
    SchoolServiceServer() : listen_fd_(-1), running_(false) {}

    ~SchoolServiceServer() {
        stop();
    }

    // Start server
    bool start(uint16_t port) {
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            return false;
        }

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = INADDR_ANY;
        addr_.sin_port = htons(port);

        if (bind(listen_fd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        if (listen(listen_fd_, 10) < 0) {
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        running_ = true;
        return true;
    }

    void stop() {
        running_ = false;
        
        // Close all client connections
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (int fd : client_fds_) {
                close(fd);
            }
            client_fds_.clear();
        }
        
        if (listen_fd_ >= 0) {
            close(listen_fd_);
            listen_fd_ = -1;
        }
        
        // Wait for all client threads to finish
        for (auto& thread : client_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        client_threads_.clear();
    }

    // Main server loop
    void run() {
        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &addr_len);

            if (client_fd < 0) {
                if (running_) {
                    continue;
                } else {
                    break;
                }
            }

            // Add client to list
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                client_fds_.push_back(client_fd);
            }

            // Handle client in a new thread
            client_threads_.emplace_back([this, client_fd]() {
                handleClient(client_fd);
                
                // Remove client from list
                {
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    auto it = std::find(client_fds_.begin(), client_fds_.end(), client_fd);
                    if (it != client_fds_.end()) {
                        client_fds_.erase(it);
                    }
                }
                
                close(client_fd);
                onClientDisconnected(client_fd);
            });
        }
    }

    // Broadcast message to all connected clients (with serialization)
    // exclude_fd: optionally exclude one client from broadcast (e.g., the sender)
    template<typename T>
    void broadcast(const T& message, int exclude_fd = -1) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        
        // Serialize message once
        ByteBuffer buffer;
        message.serialize(buffer);
        uint32_t msg_size = buffer.size();
        
        // Send to all clients except excluded one
        for (int fd : client_fds_) {
            if (fd == exclude_fd) continue; // Skip the excluded client
            // Send message size
            send(fd, &msg_size, sizeof(msg_size), 0);
            // Send message data
            send(fd, buffer.data(), buffer.size(), 0);
        }
    }

    // Get number of connected clients
    size_t getClientCount() {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return client_fds_.size();
    }

private:
    void handleClient(int client_fd) {
        onClientConnected(client_fd);
        
        while (running_) {
            // Receive message size
            uint32_t msg_size;
            ssize_t received = recv(client_fd, &msg_size, sizeof(msg_size), 0);
            
            if (received <= 0) {
                // Client disconnected or error
                break;
            }

            // Peek message ID from the data (will be consumed by handler)
            uint8_t peek_buffer[65536];
            ssize_t peeked = recv(client_fd, peek_buffer, 4, MSG_PEEK);
            if (peeked < 4) {
                break;
            }

            uint32_t msg_id = (static_cast<uint32_t>(peek_buffer[0]) << 24) |
                              (static_cast<uint32_t>(peek_buffer[1]) << 16) |
                              (static_cast<uint32_t>(peek_buffer[2]) << 8) |
                              static_cast<uint32_t>(peek_buffer[3]);

            switch (msg_id) {
                case MSG_ADDSTUDENT_REQ:
                    handle_addStudent(client_fd, msg_size);
                    break;
                case MSG_ADDTEACHER_REQ:
                    handle_addTeacher(client_fd, msg_size);
                    break;
                case MSG_GETPERSONINFO_REQ:
                    handle_getPersonInfo(client_fd, msg_size);
                    break;
                case MSG_UPDATEPERSONINFO_REQ:
                    handle_updatePersonInfo(client_fd, msg_size);
                    break;
                case MSG_REMOVEPERSON_REQ:
                    handle_removePerson(client_fd, msg_size);
                    break;
                case MSG_BATCHADDSTUDENTS_REQ:
                    handle_batchAddStudents(client_fd, msg_size);
                    break;
                case MSG_BATCHQUERYPERSONS_REQ:
                    handle_batchQueryPersons(client_fd, msg_size);
                    break;
                case MSG_ADDCOURSE_REQ:
                    handle_addCourse(client_fd, msg_size);
                    break;
                case MSG_GETALLCOURSES_REQ:
                    handle_getAllCourses(client_fd, msg_size);
                    break;
                case MSG_ENROLLCOURSE_REQ:
                    handle_enrollCourse(client_fd, msg_size);
                    break;
                case MSG_DROPCOURSE_REQ:
                    handle_dropCourse(client_fd, msg_size);
                    break;
                case MSG_SUBMITGRADE_REQ:
                    handle_submitGrade(client_fd, msg_size);
                    break;
                case MSG_GETSTUDENTGRADES_REQ:
                    handle_getStudentGrades(client_fd, msg_size);
                    break;
                case MSG_BATCHSUBMITGRADES_REQ:
                    handle_batchSubmitGrades(client_fd, msg_size);
                    break;
                case MSG_QUERYBYTYPE_REQ:
                    handle_queryByType(client_fd, msg_size);
                    break;
                case MSG_GETSTATISTICS_REQ:
                    handle_getStatistics(client_fd, msg_size);
                    break;
                case MSG_SEARCHPERSONS_REQ:
                    handle_searchPersons(client_fd, msg_size);
                    break;
                case MSG_GETTOTALCOUNT_REQ:
                    handle_getTotalCount(client_fd, msg_size);
                    break;
                case MSG_CLEARALL_REQ:
                    handle_clearAll(client_fd, msg_size);
                    break;
                default:
                    // Unknown message, consume the data
                    {
                        uint8_t discard[65536];
                        recv(client_fd, discard, msg_size, 0);
                    }
                    break;
            }
        }
    }

    void handle_addStudent(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        addStudentRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        addStudentResponse response;
        response.return_value = onaddStudent(request.student);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_addTeacher(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        addTeacherRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        addTeacherResponse response;
        response.return_value = onaddTeacher(request.teacher);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_getPersonInfo(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        getPersonInfoRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        getPersonInfoResponse response;
        response.return_value = ongetPersonInfo(request.personId);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_updatePersonInfo(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        updatePersonInfoRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        updatePersonInfoResponse response;
        response.return_value = onupdatePersonInfo(request.personId, request.info);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_removePerson(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        removePersonRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        removePersonResponse response;
        response.return_value = onremovePerson(request.personId);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_batchAddStudents(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        batchAddStudentsRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        batchAddStudentsResponse response;
        response.return_value = onbatchAddStudents(request.students);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_batchQueryPersons(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        batchQueryPersonsRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        batchQueryPersonsResponse response;
        onbatchQueryPersons(request.personIds, response.infos, response.status);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_addCourse(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        addCourseRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        addCourseResponse response;
        response.return_value = onaddCourse(request.course);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_getAllCourses(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        getAllCoursesRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        getAllCoursesResponse response;
        response.return_value = ongetAllCourses();

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_enrollCourse(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        enrollCourseRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        enrollCourseResponse response;
        response.return_value = onenrollCourse(request.studentId, request.courseId);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_dropCourse(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        dropCourseRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        dropCourseResponse response;
        response.return_value = ondropCourse(request.studentId, request.courseId);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_submitGrade(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        submitGradeRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        submitGradeResponse response;
        response.return_value = onsubmitGrade(request.grade);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_getStudentGrades(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        getStudentGradesRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        getStudentGradesResponse response;
        response.return_value = ongetStudentGrades(request.studentId);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_batchSubmitGrades(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        batchSubmitGradesRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        batchSubmitGradesResponse response;
        response.return_value = onbatchSubmitGrades(request.grades);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_queryByType(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        queryByTypeRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        queryByTypeResponse response;
        response.return_value = onqueryByType(request.personType);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_getStatistics(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        getStatisticsRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        getStatisticsResponse response;
        response.return_value = ongetStatistics();

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_searchPersons(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        searchPersonsRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        searchPersonsResponse response;
        response.return_value = onsearchPersons(request.keyword);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_getTotalCount(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        getTotalCountRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        getTotalCountResponse response;
        response.return_value = ongetTotalCount();

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_clearAll(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        clearAllRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        onclearAll();
    }

public:
    // Callback push methods (send callbacks to clients)
    void push_onPersonChanged(NotificationEvent event, int exclude_fd = -1) {
        // Prepare callback request
        onPersonChangedRequest request;
        request.event = event;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

    void push_onBatchEvents(std::vector<NotificationEvent> events, int exclude_fd = -1) {
        // Prepare callback request
        onBatchEventsRequest request;
        request.events = events;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

    void push_onSystemStatus(bool isOnline, int exclude_fd = -1) {
        // Prepare callback request
        onSystemStatusRequest request;
        request.isOnline = isOnline;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

    void push_onStatisticsUpdated(Statistics stats, int exclude_fd = -1) {
        // Prepare callback request
        onStatisticsUpdatedRequest request;
        request.stats = stats;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

protected:
    // Virtual functions to be implemented by user
    virtual OperationStatus onaddStudent(StudentDetails student) = 0;
    virtual OperationStatus onaddTeacher(TeacherDetails teacher) = 0;
    virtual PersonInfo ongetPersonInfo(const std::string& personId) = 0;
    virtual bool onupdatePersonInfo(const std::string& personId, PersonInfo info) = 0;
    virtual bool onremovePerson(const std::string& personId) = 0;
    virtual int64_t onbatchAddStudents(std::vector<StudentDetails> students) = 0;
    virtual void onbatchQueryPersons(std::vector<std::string> personIds, std::vector<PersonInfo>& infos, std::vector<OperationStatus>& status) = 0;
    virtual OperationStatus onaddCourse(Course course) = 0;
    virtual std::vector<Course> ongetAllCourses() = 0;
    virtual bool onenrollCourse(const std::string& studentId, const std::string& courseId) = 0;
    virtual bool ondropCourse(const std::string& studentId, const std::string& courseId) = 0;
    virtual bool onsubmitGrade(Grade grade) = 0;
    virtual std::vector<Grade> ongetStudentGrades(const std::string& studentId) = 0;
    virtual int64_t onbatchSubmitGrades(std::vector<Grade> grades) = 0;
    virtual std::vector<PersonInfo> onqueryByType(PersonType personType) = 0;
    virtual Statistics ongetStatistics() = 0;
    virtual std::vector<PersonInfo> onsearchPersons(const std::string& keyword) = 0;
    virtual int64_t ongetTotalCount() = 0;
    virtual void onclearAll() = 0;

    // Optional callbacks for client connection events
    virtual void onClientConnected(int client_fd) {
        // Override this to handle client connection
    }

    virtual void onClientDisconnected(int client_fd) {
        // Override this to handle client disconnection
    }
};

} // namespace ipc

#endif // SCHOOLSERVICE_SOCKET_HPP