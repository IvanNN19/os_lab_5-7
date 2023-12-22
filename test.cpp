#include <iostream>
#include <string>

int sumIntegersInString(const std::string& str) {
    int sum = 0;
    int currentNumber = 0;

    for (char ch : str) {
        if (isdigit(ch)) {
            // Если символ является цифрой, добавляем его к текущему числу
            currentNumber = currentNumber * 10 + (ch - '0');
        } else {                    
            // Если символ не цифра, добавляем текущее число к сумме и сбрасываем текущее число
            sum += currentNumber;
            currentNumber = 0;
        }
    }

    // Добавляем последнее число, если оно есть
    sum += currentNumber;

    return sum;
}

int main() {
    // Пример использования функции
    std::string inputString = "123 45 6789 1000 1000000";
    //int digitCount = 4;

    int result = sumIntegersInString(inputString);

    std::cout << "Сумма целых чисел: " << result << std::endl;

    return 0;
}