#include "pass1.cpp"

using namespace std;

ifstream intermediateFile;
ofstream errorFile, objectFile, ListingFile;

ofstream printtab;
string writestring;

bool isComment;
string label, opcode, operand, comment;
string operand1, operand2;

int lineNumber, blockNumber, address, startAddress;
string objectCode, writeData, currentRecord, modificationRecord = "M^", endRecord, write_R_Data, write_D_Data, currentSectName = "DEFAULT";
int sectionCounter = 0;
int program_section_length = 0;

int program_counter, base_register_value, currentTextRecordLength;
bool nobase;

string readTillTab(string data, int &index)
{
    string tempBuffer = "";

    while (index < data.length() && data[index] != '\t')
    {
        tempBuffer += data[index];
        index++;
    }
    index++;
    if (tempBuffer == " ")
    {
        tempBuffer = "-1";
    }
    return tempBuffer;
}

int fixed_stoi(string v2)
{

    if (v2[0] == 'X')
    {
        v2 = v2.substr(2, v2.size() - 3);
        int ret = strtol(v2.c_str(), NULL, 16);
        return ret;
    }
    else if (v2[0] == 'C')
    {
        v2 = v2.substr(2, v2.size() - 3);
        int ret = 0;
        for (int i = 0; i < v2.size(); i++)
        {
            ret = (ret << 8) + int(v2[i]);
        }
        return ret;
    }
    else
    {
        return stoi(v2);
    }
}
bool readIntermediateFile(ifstream &readFile, bool &isComment, int &lineNumber, int &address, int &blockNumber, string &label, string &opcode, string &operand, string &comment)
{
    string fileLine = "";
    string tempBuffer = "";
    bool tempStatus;
    int index = 0;
    if (!getline(readFile, fileLine))
    {
        return false;
    }

    lineNumber = fixed_stoi(readTillTab(fileLine, index));

    isComment = (fileLine[index] == '.') ? true : false;
    if (isComment)
    {
        readFirstNonWhiteSpace(fileLine, index, tempStatus, comment, true);
        return true;
    }
    address = stringHexToInt(readTillTab(fileLine, index));
    tempBuffer = readTillTab(fileLine, index);
    if (tempBuffer == " ")
    {
        blockNumber = -1;
    }
    else
    {
        blockNumber = fixed_stoi(tempBuffer);
    }
    label = readTillTab(fileLine, index);
    if (label == "-1")
    {
        label = " ";
    }
    opcode = readTillTab(fileLine, index);
    if (opcode == "BYTE")
    {
        readByteOperand(fileLine, index, tempStatus, operand);
    }
    else
    {
        operand = readTillTab(fileLine, index);
        if (operand == "-1")
        {
            operand = " ";
        }
        if (opcode == "CSECT")
        {
            return true;
        }
    }
    readFirstNonWhiteSpace(fileLine, index, tempStatus, comment, true);
    return true;
}

string createObjectCodeFormat34()
{
    string objcode;
    int halfBytes;
    halfBytes = (getFlagFormat(opcode) == '+') ? 5 : 3;

    if (getFlagFormat(operand) == '#')
    {
        if (operand.substr(operand.length() - 2, 2) == ",X")
        {
            writeData = "Line: " + to_string(lineNumber) + " Index based addressing not supported with Indirect addressing";
            writeToFile(errorFile, writeData);
            objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 1, 2);
            objcode += (halfBytes == 5) ? "100000" : "0000";
            return objcode;
        }

        string tempOperand = operand.substr(1, operand.length() - 1);
        if (if_all_num(tempOperand) || ((SYMTAB[tempOperand].exists == 'y') && (SYMTAB[tempOperand].relative == 0) && (CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'n')))
        {
            int immediateValue;

            if (if_all_num(tempOperand))
            {
                immediateValue = fixed_stoi(tempOperand);
            }
            else
            {
                immediateValue = stringHexToInt(SYMTAB[tempOperand].address);
            }
            if (immediateValue >= (1 << 4 * halfBytes))
            {
                writeData = "Line: " + to_string(lineNumber) + " Immediate value exceeds format limit";
                writeToFile(errorFile, writeData);
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 1, 2);
                objcode += (halfBytes == 5) ? "100000" : "0000";
            }
            else
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 1, 2);
                objcode += (halfBytes == 5) ? '1' : '0';
                objcode += intToStringHex(immediateValue, halfBytes);
            }
            return objcode;
        }
        else if (SYMTAB[tempOperand].exists == 'n' || CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'y')
        {

            if (CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists != 'y' || halfBytes == 3)
            {
                writeData = "Line " + to_string(lineNumber);
                if (halfBytes == 3 && CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'y')
                {
                    writeData += " : Invalid format for external reference " + tempOperand;
                }
                else
                {
                    writeData += " : Symbol doesn't exists. Found " + tempOperand;
                }
                writeToFile(errorFile, writeData);
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 1, 2);
                objcode += (halfBytes == 5) ? "100000" : "0000";
                return objcode;
            }
            if (SYMTAB[tempOperand].exists == 'y' && CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'y')
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 1, 2);
                objcode += "100000";

                modificationRecord += "M^" + intToStringHex(address + 1, 6) + '^';
                modificationRecord += "05+";
                modificationRecord += CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].name;
                modificationRecord += '\n';

                return objcode;
            }
        }
        else
        {
            int operandAddress = stringHexToInt(SYMTAB[tempOperand].address) + stringHexToInt(BLOCKS[BLocksNumToName[SYMTAB[tempOperand].blockNumber]].startAddress);

            if (halfBytes == 5)
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 1, 2);
                objcode += '1';
                objcode += intToStringHex(operandAddress, halfBytes);

                modificationRecord += "M^" + intToStringHex(address + 1, 6) + '^';
                modificationRecord += (halfBytes == 5) ? "05" : "03";
                modificationRecord += '\n';

                return objcode;
            }

            program_counter = address + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress);
            program_counter += (halfBytes == 5) ? 4 : 3;
            int relativeAddress = operandAddress - program_counter;

            if (relativeAddress >= (-2048) && relativeAddress <= 2047)
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 1, 2);
                objcode += '2';
                objcode += intToStringHex(relativeAddress, halfBytes);
                return objcode;
            }

            if (!nobase)
            {
                relativeAddress = operandAddress - base_register_value;
                if (relativeAddress >= 0 && relativeAddress <= 4095)
                {
                    objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 1, 2);
                    objcode += '4';
                    objcode += intToStringHex(relativeAddress, halfBytes);
                    return objcode;
                }
            }

            if (operandAddress <= 4095)
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 1, 2);
                objcode += '0';
                objcode += intToStringHex(operandAddress, halfBytes);

                modificationRecord += "M^" + intToStringHex(address + 1 + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress), 6) + '^';
                modificationRecord += (halfBytes == 5) ? "05" : "03";
                modificationRecord += '\n';

                return objcode;
            }
        }
    }
    else if (getFlagFormat(operand) == '@')
    {
        string tempOperand = operand.substr(1, operand.length() - 1);
        if (tempOperand.substr(tempOperand.length() - 2, 2) == ",X" || SYMTAB[tempOperand].exists == 'n' || CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'y')
        {

            if (CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists != 'y' || halfBytes == 3)
            {
                writeData = "Line " + to_string(lineNumber);
                if (halfBytes == 3 && CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'y')
                {
                    writeData += " : Invalid format for external reference " + tempOperand;
                }
                else
                {
                    writeData += " : Symbol doesn't exists.Index based addressing not supported with Indirect addressing ";
                }
                writeToFile(errorFile, writeData);
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 2, 2);
                objcode += (halfBytes == 5) ? "100000" : "0000";
                return objcode;
            }

            if (SYMTAB[tempOperand].exists == 'y' && CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'y')
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 2, 2);
                objcode += "100000";

                modificationRecord += "M^" + intToStringHex(address + 1, 6) + '^';
                modificationRecord += "05+";
                modificationRecord += CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].name;
                modificationRecord += '\n';

                return objcode;
            }
        }
        int operandAddress = stringHexToInt(SYMTAB[tempOperand].address) + stringHexToInt(BLOCKS[BLocksNumToName[SYMTAB[tempOperand].blockNumber]].startAddress);
        program_counter = address + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress);
        program_counter += (halfBytes == 5) ? 4 : 3;

        if (halfBytes == 3)
        {
            int relativeAddress = operandAddress - program_counter;
            if (relativeAddress >= (-2048) && relativeAddress <= 2047)
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 2, 2);
                objcode += '2';
                objcode += intToStringHex(relativeAddress, halfBytes);
                return objcode;
            }

            if (!nobase)
            {
                relativeAddress = operandAddress - base_register_value;
                if (relativeAddress >= 0 && relativeAddress <= 4095)
                {
                    objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 2, 2);
                    objcode += '4';
                    objcode += intToStringHex(relativeAddress, halfBytes);
                    return objcode;
                }
            }

            if (operandAddress <= 4095)
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 2, 2);
                objcode += '0';
                objcode += intToStringHex(operandAddress, halfBytes);

                modificationRecord += "M^" + intToStringHex(address + 1 + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress), 6) + '^';
                modificationRecord += (halfBytes == 5) ? "05" : "03";
                modificationRecord += '\n';

                return objcode;
            }
        }
        else
        {
            objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 2, 2);
            objcode += '1';
            objcode += intToStringHex(operandAddress, halfBytes);

            modificationRecord += "M^" + intToStringHex(address + 1 + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress), 6) + '^';
            modificationRecord += (halfBytes == 5) ? "05" : "03";
            modificationRecord += '\n';

            return objcode;
        }

        writeData = "Line: " + to_string(lineNumber);
        writeData += "Can't fit into program counter based or base register based addressing.";
        writeToFile(errorFile, writeData);
        objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 2, 2);
        objcode += (halfBytes == 5) ? "100000" : "0000";

        return objcode;
    }
    else if (getFlagFormat(operand) == '=')
    {
        string tempOperand = operand.substr(1, operand.length() - 1);

        if (tempOperand == "*")
        {
            tempOperand = "X'" + intToStringHex(address, 6) + "'";

            modificationRecord += "M^" + intToStringHex(stringHexToInt(LITTAB[tempOperand].address) + stringHexToInt(BLOCKS[BLocksNumToName[LITTAB[tempOperand].blockNumber]].startAddress), 6) + '^';
            modificationRecord += intToStringHex(6, 2);
            modificationRecord += '\n';
        }

        if (LITTAB[tempOperand].exists == 'n')
        {
            writeData = "Line " + to_string(lineNumber) + " : Symbol doesn't exists. Found " + tempOperand;
            writeToFile(errorFile, writeData);

            objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
            objcode += (halfBytes == 5) ? "000" : "0";
            objcode += "000";
            return objcode;
        }

        int operandAddress = stringHexToInt(LITTAB[tempOperand].address) + stringHexToInt(BLOCKS[BLocksNumToName[LITTAB[tempOperand].blockNumber]].startAddress);
        program_counter = address + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress);
        program_counter += (halfBytes == 5) ? 4 : 3;

        if (halfBytes == 3)
        {
            int relativeAddress = operandAddress - program_counter;
            if (relativeAddress >= (-2048) && relativeAddress <= 2047)
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
                objcode += '2';
                objcode += intToStringHex(relativeAddress, halfBytes);
                return objcode;
            }

            if (!nobase)
            {
                relativeAddress = operandAddress - base_register_value;
                if (relativeAddress >= 0 && relativeAddress <= 4095)
                {
                    objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
                    objcode += '4';
                    objcode += intToStringHex(relativeAddress, halfBytes);
                    return objcode;
                }
            }

            if (operandAddress <= 4095)
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
                objcode += '0';
                objcode += intToStringHex(operandAddress, halfBytes);

                /*add modifacation record here*/
                modificationRecord += "M^" + intToStringHex(address + 1 + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress), 6) + '^';
                modificationRecord += (halfBytes == 5) ? "05" : "03";
                modificationRecord += '\n';

                return objcode;
            }
        }
        else
        {
            objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
            objcode += '1';
            objcode += intToStringHex(operandAddress, halfBytes);

            modificationRecord += "M^" + intToStringHex(address + 1 + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress), 6) + '^';
            modificationRecord += (halfBytes == 5) ? "05" : "03";
            modificationRecord += '\n';

            return objcode;
        }

        writeData = "Line: " + to_string(lineNumber);
        writeData += "Can't fit into program counter based or base register based addressing.";
        writeToFile(errorFile, writeData);
        objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
        objcode += (halfBytes == 5) ? "100" : "0";
        objcode += "000";

        return objcode;
    }
    else
    {
        int xbpe = 0;
        string tempOperand = operand;
        if (operand.substr(operand.length() - 2, 2) == ",X")
        {
            tempOperand = operand.substr(0, operand.length() - 2);
            xbpe = 8;
        }

        if (SYMTAB[tempOperand].exists == 'n' || CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'y')
        {

            if (CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists != 'y' || halfBytes == 3)
            {

                writeData = "Line " + to_string(lineNumber);
                if (halfBytes == 3 && CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'y')
                {
                    writeData += " : Invalid format for external reference " + tempOperand;
                }
                else
                {
                    writeData += " : Symbol doesn't exists. Found " + tempOperand;
                }
                writeToFile(errorFile, writeData);
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
                objcode += (halfBytes == 5) ? (intToStringHex(xbpe + 1, 1) + "00") : intToStringHex(xbpe, 1);
                objcode += "000";
                return objcode;
            }

            if (SYMTAB[tempOperand].exists == 'y' && CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].exists == 'y')
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
                objcode += "100000";

                modificationRecord += "M^" + intToStringHex(address + 1, 6) + '^';
                modificationRecord += "05+";
                modificationRecord += CSECT_TAB[currentSectName].EXTREF_TAB[tempOperand].name;
                modificationRecord += '\n';

                return objcode;
            }
        }
        else
        {
            int operandAddress = stringHexToInt(SYMTAB[tempOperand].address) + stringHexToInt(BLOCKS[BLocksNumToName[SYMTAB[tempOperand].blockNumber]].startAddress);
            program_counter = address + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress);
            program_counter += (halfBytes == 5) ? 4 : 3;

            if (halfBytes == 3)
            {
                int relativeAddress = operandAddress - program_counter;
                if (relativeAddress >= (-2048) && relativeAddress <= 2047)
                {
                    objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
                    objcode += intToStringHex(xbpe + 2, 1);
                    objcode += intToStringHex(relativeAddress, halfBytes);
                    return objcode;
                }

                if (!nobase)
                {
                    relativeAddress = operandAddress - base_register_value;
                    if (relativeAddress >= 0 && relativeAddress <= 4095)
                    {
                        objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
                        objcode += intToStringHex(xbpe + 4, 1);
                        objcode += intToStringHex(relativeAddress, halfBytes);
                        return objcode;
                    }
                }

                if (operandAddress <= 4095)
                {
                    objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
                    objcode += intToStringHex(xbpe, 1);
                    objcode += intToStringHex(operandAddress, halfBytes);

                    /*add modifacation record here*/
                    modificationRecord += "M^" + intToStringHex(address + 1 + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress), 6) + '^';
                    modificationRecord += (halfBytes == 5) ? "05" : "03";
                    modificationRecord += '\n';

                    return objcode;
                }
            }
            else
            {
                objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
                objcode += intToStringHex(xbpe + 1, 1);
                objcode += intToStringHex(operandAddress, halfBytes);

                modificationRecord += "M^" + intToStringHex(address + 1 + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress), 6) + '^';
                modificationRecord += (halfBytes == 5) ? "05" : "03";
                modificationRecord += '\n';

                return objcode;
            }

            writeData = "Line: " + to_string(lineNumber);
            writeData += "Can't fit into program counter based or base register based addressing.";
            writeToFile(errorFile, writeData);
            objcode = intToStringHex(stringHexToInt(OPTAB[getRealOpcode(opcode)].opcode) + 3, 2);
            objcode += (halfBytes == 5) ? (intToStringHex(xbpe + 1, 1) + "00") : intToStringHex(xbpe, 1);
            objcode += "000";

            return objcode;
        }
    }
}

void writeTextRecord(bool lastRecord = false)
{
    if (lastRecord)
    {
        if (currentRecord.length() > 0)
        {
            writeData = intToStringHex(currentRecord.length() / 2, 2) + '^' + currentRecord;
            writeToFile(objectFile, writeData);
            currentRecord = "";
        }
        return;
    }
    if (objectCode != "")
    {
        if (currentRecord.length() == 0)
        {
            writeData = "T^" + intToStringHex(address + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress), 6) + '^';
            writeToFile(objectFile, writeData, false);
        }

        if ((currentRecord + objectCode).length() > 60)
        {
            writeData = intToStringHex(currentRecord.length() / 2, 2) + '^' + currentRecord;
            writeToFile(objectFile, writeData);

            currentRecord = "";
            writeData = "T^" + intToStringHex(address + stringHexToInt(BLOCKS[BLocksNumToName[blockNumber]].startAddress), 6) + '^';
            writeToFile(objectFile, writeData, false);
        }

        currentRecord += objectCode;
    }
    else
    {
        if (opcode == "START" || opcode == "END" || opcode == "BASE" || opcode == "NOBASE" || opcode == "LTORG" || opcode == "ORG" || opcode == "EQU" || opcode == "EXTREF" || opcode == "EXTDEF")
        {
        }
        else
        {
            if (currentRecord.length() > 0)
            {
                writeData = intToStringHex(currentRecord.length() / 2, 2) + '^' + currentRecord;
                writeToFile(objectFile, writeData);
            }
            currentRecord = "";
        }
    }
}

void writeDRecord()
{
    write_D_Data = "D^";
    string temp_address = "";
    int length = operand.length();
    string inp = "";
    for (int i = 0; i < length; i++)
    {
        while (operand[i] != ',' && i < length)
        {
            inp += operand[i];
            i++;
        }
        temp_address = CSECT_TAB[currentSectName].EXTDEF_TAB[inp].address;
        write_D_Data += expandString(inp, 6, ' ', true) + temp_address;
        inp = "";
    }
    writeToFile(objectFile, write_D_Data);
}

void writeRRecord()
{
    write_R_Data = "R^";
    string temp_address = "";
    int length = operand.length();
    string inp = "";
    for (int i = 0; i < length; i++)
    {
        while (operand[i] != ',' && i < length)
        {
            inp += operand[i];
            i++;
        }
        write_R_Data += expandString(inp, 6, ' ', true);
        inp = "";
    }
    writeToFile(objectFile, write_R_Data);
}

void writeEndRecord(bool write = true)
{
    if (write)
    {
        if (endRecord.length() > 0)
        {
            writeToFile(objectFile, endRecord);
        }
        else
        {
            writeEndRecord(false);
        }
    }
    if ((operand == "" || operand == " ") && currentSectName == "DEFAULT")
    {
        endRecord = "E^" + intToStringHex(startAddress, 6);
    }
    else if (currentSectName != "DEFAULT")
    {
        endRecord = "E";
    }
    else
    {
        int firstExecutableAddress;
        firstExecutableAddress = stringHexToInt(SYMTAB[firstExecutable_Sec].address);
        endRecord = "E^" + intToStringHex(firstExecutableAddress, 6) + "\n";
    }
}

void pass2()
{
    string tempBuffer;
    intermediateFile.open("intermediate.txt");
    if (!intermediateFile)
    {
        cout << "Unable to open file: intermediate.txt" << endl;
        exit(1);
    }
    getline(intermediateFile, tempBuffer);

    objectFile.open("object_program.txt");
    if (!objectFile)
    {
        cout << "Unable to open file: object_program.txt" << endl;
        exit(1);
    }

    ListingFile.open("listing.txt");
    if (!ListingFile)
    {
        cout << "Unable to open file: listing.txt" << endl;
        exit(1);
    }
    writeToFile(ListingFile, "Line\tAddress\tLabel\tOPCODE\tOPERAND\tObjectCode\tComment");

    errorFile.open("error.txt", fstream::app);
    if (!errorFile)
    {
        cout << "Unable to open file: error.txt" << endl;
        exit(1);
    }
    writeToFile(errorFile, "\n--- PASS 2 ---");
    objectCode = "";
    currentTextRecordLength = 0;
    currentRecord = "";
    modificationRecord = "";
    blockNumber = 0;
    nobase = true;

    readIntermediateFile(intermediateFile, isComment, lineNumber, address, blockNumber, label, opcode, operand, comment);
    while (isComment)
    {
        writeData = to_string(lineNumber) + "\t" + comment;
        writeToFile(ListingFile, writeData);
        readIntermediateFile(intermediateFile, isComment, lineNumber, address, blockNumber, label, opcode, operand, comment);
    }

    if (opcode == "START")
    {
        startAddress = address;
        writeData = to_string(lineNumber) + "\t" + intToStringHex(address) + "\t" + to_string(blockNumber) + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + objectCode + "\t" + comment;
        writeToFile(ListingFile, writeData);
    }
    else
    {
        label = "";
        startAddress = 0;
        address = 0;
    }
    if (BLOCKS.size() > 1)
    {
        program_section_length = program_length;
    }
    else
    {
        program_section_length = CSECT_TAB[currentSectName].length;
    }

    writeData = "H^" + expandString(label, 6, ' ', true) + '^' + intToStringHex(address, 6) + '^' + intToStringHex(program_section_length, 6);
    writeToFile(objectFile, writeData);

    readIntermediateFile(intermediateFile, isComment, lineNumber, address, blockNumber, label, opcode, operand, comment);
    currentTextRecordLength = 0;

    while (opcode != "END")
    {
        while (opcode != "END" && opcode != "CSECT")
        {
            if (!isComment)
            {
                if (OPTAB[getRealOpcode(opcode)].exists == 'y')
                {
                    if (OPTAB[getRealOpcode(opcode)].format == 1)
                    {
                        objectCode = OPTAB[getRealOpcode(opcode)].opcode;
                    }
                    else if (OPTAB[getRealOpcode(opcode)].format == 2)
                    {
                        operand1 = operand.substr(0, operand.find(','));
                        operand2 = operand.substr(operand.find(',') + 1, operand.length() - operand.find(',') - 1);

                        if (operand2 == operand)
                        {
                            if (getRealOpcode(opcode) == "SVC")
                            {
                                objectCode = OPTAB[getRealOpcode(opcode)].opcode + intToStringHex(fixed_stoi(operand1), 1) + '0';
                            }
                            else if (REGTAB[operand1].exists == 'y')
                            {
                                objectCode = OPTAB[getRealOpcode(opcode)].opcode + REGTAB[operand1].num + '0';
                            }
                            else
                            {
                                objectCode = getRealOpcode(opcode) + '0' + '0';
                                writeData = "Line: " + to_string(lineNumber) + " Invalid Register name";
                                writeToFile(errorFile, writeData);
                            }
                        }
                        else
                        {
                            if (REGTAB[operand1].exists == 'n')
                            {
                                objectCode = OPTAB[getRealOpcode(opcode)].opcode + "00";
                                writeData = "Line: " + to_string(lineNumber) + " Invalid Register name";
                                writeToFile(errorFile, writeData);
                            }
                            else if (getRealOpcode(opcode) == "SHIFTR" || getRealOpcode(opcode) == "SHIFTL")
                            {
                                objectCode = OPTAB[getRealOpcode(opcode)].opcode + REGTAB[operand1].num + intToStringHex(fixed_stoi(operand2), 1);
                            }
                            else if (REGTAB[operand2].exists == 'n')
                            {
                                objectCode = OPTAB[getRealOpcode(opcode)].opcode + "00";
                                writeData = "Line: " + to_string(lineNumber) + " Invalid Register name";
                                writeToFile(errorFile, writeData);
                            }
                            else
                            {
                                objectCode = OPTAB[getRealOpcode(opcode)].opcode + REGTAB[operand1].num + REGTAB[operand2].num;
                            }
                        }
                    }
                    else if (OPTAB[getRealOpcode(opcode)].format == 3)
                    {
                        if (getRealOpcode(opcode) == "RSUB")
                        {
                            objectCode = OPTAB[getRealOpcode(opcode)].opcode;
                            objectCode += (getFlagFormat(opcode) == '+') ? "000000" : "0000";
                        }
                        else
                        {
                            objectCode = createObjectCodeFormat34();
                        }
                    }
                }
                else if (opcode == "BYTE")
                {
                    if (operand[0] == 'X')
                    {
                        objectCode = operand.substr(2, operand.length() - 3);
                    }
                    else if (operand[0] == 'C')
                    {
                        objectCode = stringToHexString(operand.substr(2, operand.length() - 3));
                    }
                }
                else if (label == "*")
                {
                    if (opcode[1] == 'C')
                    {
                        objectCode = stringToHexString(opcode.substr(3, opcode.length() - 4));
                    }
                    else if (opcode[1] == 'X')
                    {
                        objectCode = opcode.substr(3, opcode.length() - 4);
                    }
                }
                else if (opcode == "WORD")
                {
                    objectCode = intToStringHex(fixed_stoi(operand), 6);
                }
                else if (opcode == "BASE")
                {
                    if (SYMTAB[operand].exists == 'y')
                    {
                        base_register_value = stringHexToInt(SYMTAB[operand].address) + stringHexToInt(BLOCKS[BLocksNumToName[SYMTAB[operand].blockNumber]].startAddress);
                        nobase = false;
                    }
                    else
                    {
                        writeData = "Line " + to_string(lineNumber) + " : Symbol doesn't exists. Found " + operand;
                        writeToFile(errorFile, writeData);
                    }
                    objectCode = "";
                }
                else if (opcode == "NOBASE")
                {
                    if (nobase)
                    {
                        writeData = "Line " + to_string(lineNumber) + ": Assembler wasn't using base addressing";
                        writeToFile(errorFile, writeData);
                    }
                    else
                    {
                        nobase = true;
                    }
                    objectCode = "";
                }
                else
                {
                    objectCode = "";
                }
                writeTextRecord();
                if (blockNumber == -1 && address != -1)
                {
                    writeData = to_string(lineNumber) + "\t" + intToStringHex(address) + "\t" + " " + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + objectCode + "\t" + comment;
                }
                else if (address == -1)
                {
                    if (opcode == "EXTDEF")
                    {
                        writeDRecord();
                    }
                    else if (opcode == "EXTREF")
                    {
                        writeRRecord();
                    }
                    writeData = to_string(lineNumber) + "\t" + " " + "\t" + " " + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + objectCode + "\t" + comment;
                }

                else
                {
                    writeData = to_string(lineNumber) + "\t" + intToStringHex(address) + "\t" + to_string(blockNumber) + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + objectCode + "\t" + comment;
                }
            }
            else
            {
                writeData = to_string(lineNumber) + "\t" + comment;
            }
            writeToFile(ListingFile, writeData);
            readIntermediateFile(intermediateFile, isComment, lineNumber, address, blockNumber, label, opcode, operand, comment);
            objectCode = "";
        }

        writeTextRecord();

        writeEndRecord(false);

        if (opcode == "CSECT" && !isComment)
        {
            writeData = to_string(lineNumber) + "\t" + intToStringHex(address) + "\t" + to_string(blockNumber) + "\t" + label + "\t" + opcode + "\t" + " " + "\t" + objectCode + "\t" + " ";
        }
        else if (!isComment)
        {
            writeData = to_string(lineNumber) + "\t" + intToStringHex(address) + "\t" + " " + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + "" + "\t" + comment;
        }
        else
        {
            writeData = to_string(lineNumber) + "\t" + comment;
        }
        writeToFile(ListingFile, writeData);

        if (opcode != "CSECT")
        {
            while (readIntermediateFile(intermediateFile, isComment, lineNumber, address, blockNumber, label, opcode, operand, comment))
            {
                if (label == "*")
                {
                    if (opcode[1] == 'C')
                    {
                        objectCode = stringToHexString(opcode.substr(3, opcode.length() - 4));
                    }
                    else if (opcode[1] == 'X')
                    {
                        objectCode = opcode.substr(3, opcode.length() - 4);
                    }
                    writeTextRecord();
                }
                writeData = to_string(lineNumber) + "\t" + intToStringHex(address) + "\t" + to_string(blockNumber) + label + "\t" + opcode + "\t" + operand + "\t" + objectCode + "\t" + comment;
                writeToFile(ListingFile, writeData);
            }
        }

        writeTextRecord(true);
        if (!isComment)
        {

            writeToFile(objectFile, modificationRecord, false);
            writeEndRecord(true);
            currentSectName = label;
            modificationRecord = "";
        }
        if (!isComment && opcode != "END")
        {
            writeData = "\n--- Object Program for " + label + " ---";
            writeToFile(objectFile, writeData);

            writeData = "\nH^" + expandString(label, 6, ' ', true) + '^' + intToStringHex(address, 6) + '^' + intToStringHex(CSECT_TAB[label].length, 6);
            writeToFile(objectFile, writeData);
        }
        readIntermediateFile(intermediateFile, isComment, lineNumber, address, blockNumber, label, opcode, operand, comment);
        objectCode = "";
    }
}

int main()
{
    cout << "Note: The input code and executable of the assembler should be in the same directory." << endl
         << endl;
    cout << "Enter name of the input code file: ";
    cin >> fileName;

    cout << "\nLoading OPTAB" << endl;
    load_tables();

    cout << "\n\tPerforming PASS 1" << endl;
    cout << "Writing intermediate_File to 'intermediate.txt'" << endl;
    cout << "Writing error_File to 'error.txt'" << endl;
    pass1();

    cout << "Writing SYMBOL TABLE" << endl;
    printtab.open("tables.txt");
    writeToFile(printtab, "--- SYMBOL TABLE ---\n");
    for (auto const &it : SYMTAB)
    {
        writestring += it.first + ":-\t" + "name:" + it.second.name + "\t|" + "address:" + it.second.address + "\t|" + "relative:" + intToStringHex(it.second.relative) + " \n";
    }
    writeToFile(printtab, writestring);

    writestring = "";
    cout << "Writing LITERAL TABLE" << endl;

    writeToFile(printtab, "--- LITERAL TABLE ---\n");
    for (auto const &it : LITTAB)
    {
        writestring += it.first + ":-\t" + "value:" + it.second.value + "\t|" + "address:" + it.second.address + " \n";
    }
    writeToFile(printtab, writestring);

    writestring = "";
    cout << "Writing EXTREF TABLE" << endl;

    writeToFile(printtab, "--- EXTREF TABLE ---\n");
    for (auto const &it0 : CSECT_TAB)
    {
        for (auto const &it : it0.second.EXTREF_TAB)
            writestring += it.first + ":-\t" + "name:" + it.second.name + "\t|" + it0.second.name + " \n";
    }
    writeToFile(printtab, writestring);

    writestring = "";
    cout << "Writing EXTDEF TABLE" << endl;

    writeToFile(printtab, "--- EXTDEF TABLE ---\n");
    for (auto const &it0 : CSECT_TAB)
    {
        for (auto const &it : it0.second.EXTDEF_TAB)
        {
            if (it.second.name != "undefined")
                writestring += it.first + ":-\t" + "name:" + it.second.name + "\t|" + "address:" + it.second.address + "\t|" + " \n";
        }
    }

    writeToFile(printtab, writestring);

    cout << "\n\tPerforming PASS 2" << endl;
    cout << "Writing object_File to 'object_program.txt'" << endl;
    cout << "Writing listing_File to 'listing.txt'" << endl;
    pass2();
}