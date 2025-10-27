// Example server implementation for SchoolService
#include "schoolservice_socket.hpp"
#include <iostream>

using namespace ipc;

class MySchoolServiceServer : public ipc::SchoolServiceServer {
protected:
    OperationStatus onaddStudent(StudentDetails student) override {
        // TODO: Implement addStudent
        std::cout << "addStudent called" << std::endl;
        return OperationStatus();
    }

    OperationStatus onaddTeacher(TeacherDetails teacher) override {
        // TODO: Implement addTeacher
        std::cout << "addTeacher called" << std::endl;
        return OperationStatus();
    }

    PersonInfo ongetPersonInfo(const std::string& personId) override {
        // TODO: Implement getPersonInfo
        std::cout << "getPersonInfo called" << std::endl;
        return PersonInfo();
    }

    bool onupdatePersonInfo(const std::string& personId, PersonInfo info) override {
        // TODO: Implement updatePersonInfo
        std::cout << "updatePersonInfo called" << std::endl;
        return bool();
    }

    bool onremovePerson(const std::string& personId) override {
        // TODO: Implement removePerson
        std::cout << "removePerson called" << std::endl;
        return bool();
    }

    int64_t onbatchAddStudents(std::vector<StudentDetails> students) override {
        // TODO: Implement batchAddStudents
        std::cout << "batchAddStudents called" << std::endl;
        return int64_t();
    }

    void onbatchQueryPersons(std::vector<std::string> personIds, std::vector<PersonInfo>& infos, std::vector<OperationStatus>& status) override {
        // TODO: Implement batchQueryPersons
        std::cout << "batchQueryPersons called" << std::endl;
    }

    OperationStatus onaddCourse(Course course) override {
        // TODO: Implement addCourse
        std::cout << "addCourse called" << std::endl;
        return OperationStatus();
    }

    std::vector<Course> ongetAllCourses() override {
        // TODO: Implement getAllCourses
        std::cout << "getAllCourses called" << std::endl;
        return std::vector<Course>();
    }

    bool onenrollCourse(const std::string& studentId, const std::string& courseId) override {
        // TODO: Implement enrollCourse
        std::cout << "enrollCourse called" << std::endl;
        return bool();
    }

    bool ondropCourse(const std::string& studentId, const std::string& courseId) override {
        // TODO: Implement dropCourse
        std::cout << "dropCourse called" << std::endl;
        return bool();
    }

    bool onsubmitGrade(Grade grade) override {
        // TODO: Implement submitGrade
        std::cout << "submitGrade called" << std::endl;
        return bool();
    }

    std::vector<Grade> ongetStudentGrades(const std::string& studentId) override {
        // TODO: Implement getStudentGrades
        std::cout << "getStudentGrades called" << std::endl;
        return std::vector<Grade>();
    }

    int64_t onbatchSubmitGrades(std::vector<Grade> grades) override {
        // TODO: Implement batchSubmitGrades
        std::cout << "batchSubmitGrades called" << std::endl;
        return int64_t();
    }

    std::vector<PersonInfo> onqueryByType(PersonType personType) override {
        // TODO: Implement queryByType
        std::cout << "queryByType called" << std::endl;
        return std::vector<PersonInfo>();
    }

    Statistics ongetStatistics() override {
        // TODO: Implement getStatistics
        std::cout << "getStatistics called" << std::endl;
        return Statistics();
    }

    std::vector<PersonInfo> onsearchPersons(const std::string& keyword) override {
        // TODO: Implement searchPersons
        std::cout << "searchPersons called" << std::endl;
        return std::vector<PersonInfo>();
    }

    int64_t ongetTotalCount() override {
        // TODO: Implement getTotalCount
        std::cout << "getTotalCount called" << std::endl;
        return int64_t();
    }

    void onclearAll() override {
        // TODO: Implement clearAll
        std::cout << "clearAll called" << std::endl;
    }

};

int main() {
    MySchoolServiceServer server;

    if (!server.start(8888)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Server started on port 8888" << std::endl;
    server.run();

    return 0;
}