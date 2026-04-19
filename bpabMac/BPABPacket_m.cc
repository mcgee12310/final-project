//
// Generated file, do not edit! Created by nedtool 4.6 from src/node/communication/mac/bpabMac/BPABPacket.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#include <iostream>
#include <sstream>
#include "BPABPacket_m.h"

USING_NAMESPACE


// Another default rule (prevents compiler from choosing base class' doPacking())
template<typename T>
void doPacking(cCommBuffer *, T& t) {
    throw cRuntimeError("Parsim error: no doPacking() function for type %s or its base class (check .msg and _m.cc/h files!)",opp_typename(typeid(t)));
}

template<typename T>
void doUnpacking(cCommBuffer *, T& t) {
    throw cRuntimeError("Parsim error: no doUnpacking() function for type %s or its base class (check .msg and _m.cc/h files!)",opp_typename(typeid(t)));
}




// Template rule for outputting std::vector<T> types
template<typename T, typename A>
inline std::ostream& operator<<(std::ostream& out, const std::vector<T,A>& vec)
{
    out.put('{');
    for(typename std::vector<T,A>::const_iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (it != vec.begin()) {
            out.put(','); out.put(' ');
        }
        out << *it;
    }
    out.put('}');
    
    char buf[32];
    sprintf(buf, " (size=%u)", (unsigned int)vec.size());
    out.write(buf, strlen(buf));
    return out;
}

// Template rule which fires if a struct or class doesn't have operator<<
template<typename T>
inline std::ostream& operator<<(std::ostream& out,const T&) {return out;}

EXECUTE_ON_STARTUP(
    cEnum *e = cEnum::find("BPABMessageType");
    if (!e) enums.getInstance()->add(e = new cEnum("BPABMessageType"));
    e->insert(BPAB_RTB, "BPAB_RTB");
    e->insert(BPAB_BLACK_BURST, "BPAB_BLACK_BURST");
    e->insert(BPAB_CTB, "BPAB_CTB");
    e->insert(BPAB_DATA, "BPAB_DATA");
);

EXECUTE_ON_STARTUP(
    cEnum *e = cEnum::find("direction");
    if (!e) enums.getInstance()->add(e = new cEnum("direction"));
    e->insert(EAST, "EAST");
    e->insert(WEST, "WEST");
    e->insert(NORTH, "NORTH");
    e->insert(SOUTH, "SOUTH");
);

Register_Class(BPABPacket);

BPABPacket::BPABPacket(const char *name, int kind) : ::MacPacket(name,kind)
{
    this->bpabType_var = 0;
    this->direction_var = 0;
    this->rtbSentTime_var = 0;
    this->sourceId_var = 0;
    this->destinationId_var = 0;
    this->sourceX_var = 0;
    this->sourceY_var = 0;
    this->iteration_var = 0;
    this->maxIterations_var = 4;
    this->rangeR_var = 400.0;
    this->limitL_var = 0;
    this->limitU_var = 0;
    this->payload_var = 0;
}

BPABPacket::BPABPacket(const BPABPacket& other) : ::MacPacket(other)
{
    copy(other);
}

BPABPacket::~BPABPacket()
{
}

BPABPacket& BPABPacket::operator=(const BPABPacket& other)
{
    if (this==&other) return *this;
    ::MacPacket::operator=(other);
    copy(other);
    return *this;
}

void BPABPacket::copy(const BPABPacket& other)
{
    this->bpabType_var = other.bpabType_var;
    this->direction_var = other.direction_var;
    this->rtbSentTime_var = other.rtbSentTime_var;
    this->sourceId_var = other.sourceId_var;
    this->destinationId_var = other.destinationId_var;
    this->sourceX_var = other.sourceX_var;
    this->sourceY_var = other.sourceY_var;
    this->iteration_var = other.iteration_var;
    this->maxIterations_var = other.maxIterations_var;
    this->rangeR_var = other.rangeR_var;
    this->limitL_var = other.limitL_var;
    this->limitU_var = other.limitU_var;
    this->payload_var = other.payload_var;
}

void BPABPacket::parsimPack(cCommBuffer *b)
{
    ::MacPacket::parsimPack(b);
    doPacking(b,this->bpabType_var);
    doPacking(b,this->direction_var);
    doPacking(b,this->rtbSentTime_var);
    doPacking(b,this->sourceId_var);
    doPacking(b,this->destinationId_var);
    doPacking(b,this->sourceX_var);
    doPacking(b,this->sourceY_var);
    doPacking(b,this->iteration_var);
    doPacking(b,this->maxIterations_var);
    doPacking(b,this->rangeR_var);
    doPacking(b,this->limitL_var);
    doPacking(b,this->limitU_var);
    doPacking(b,this->payload_var);
}

void BPABPacket::parsimUnpack(cCommBuffer *b)
{
    ::MacPacket::parsimUnpack(b);
    doUnpacking(b,this->bpabType_var);
    doUnpacking(b,this->direction_var);
    doUnpacking(b,this->rtbSentTime_var);
    doUnpacking(b,this->sourceId_var);
    doUnpacking(b,this->destinationId_var);
    doUnpacking(b,this->sourceX_var);
    doUnpacking(b,this->sourceY_var);
    doUnpacking(b,this->iteration_var);
    doUnpacking(b,this->maxIterations_var);
    doUnpacking(b,this->rangeR_var);
    doUnpacking(b,this->limitL_var);
    doUnpacking(b,this->limitU_var);
    doUnpacking(b,this->payload_var);
}

int BPABPacket::getBpabType() const
{
    return bpabType_var;
}

void BPABPacket::setBpabType(int bpabType)
{
    this->bpabType_var = bpabType;
}

int BPABPacket::getDirection() const
{
    return direction_var;
}

void BPABPacket::setDirection(int direction)
{
    this->direction_var = direction;
}

double BPABPacket::getRtbSentTime() const
{
    return rtbSentTime_var;
}

void BPABPacket::setRtbSentTime(double rtbSentTime)
{
    this->rtbSentTime_var = rtbSentTime;
}

int BPABPacket::getSourceId() const
{
    return sourceId_var;
}

void BPABPacket::setSourceId(int sourceId)
{
    this->sourceId_var = sourceId;
}

int BPABPacket::getDestinationId() const
{
    return destinationId_var;
}

void BPABPacket::setDestinationId(int destinationId)
{
    this->destinationId_var = destinationId;
}

double BPABPacket::getSourceX() const
{
    return sourceX_var;
}

void BPABPacket::setSourceX(double sourceX)
{
    this->sourceX_var = sourceX;
}

double BPABPacket::getSourceY() const
{
    return sourceY_var;
}

void BPABPacket::setSourceY(double sourceY)
{
    this->sourceY_var = sourceY;
}

int BPABPacket::getIteration() const
{
    return iteration_var;
}

void BPABPacket::setIteration(int iteration)
{
    this->iteration_var = iteration;
}

int BPABPacket::getMaxIterations() const
{
    return maxIterations_var;
}

void BPABPacket::setMaxIterations(int maxIterations)
{
    this->maxIterations_var = maxIterations;
}

double BPABPacket::getRangeR() const
{
    return rangeR_var;
}

void BPABPacket::setRangeR(double rangeR)
{
    this->rangeR_var = rangeR;
}

double BPABPacket::getLimitL() const
{
    return limitL_var;
}

void BPABPacket::setLimitL(double limitL)
{
    this->limitL_var = limitL;
}

double BPABPacket::getLimitU() const
{
    return limitU_var;
}

void BPABPacket::setLimitU(double limitU)
{
    this->limitU_var = limitU;
}

const char * BPABPacket::getPayload() const
{
    return payload_var.c_str();
}

void BPABPacket::setPayload(const char * payload)
{
    this->payload_var = payload;
}

class BPABPacketDescriptor : public cClassDescriptor
{
  public:
    BPABPacketDescriptor();
    virtual ~BPABPacketDescriptor();

    virtual bool doesSupport(cObject *obj) const;
    virtual const char *getProperty(const char *propertyname) const;
    virtual int getFieldCount(void *object) const;
    virtual const char *getFieldName(void *object, int field) const;
    virtual int findField(void *object, const char *fieldName) const;
    virtual unsigned int getFieldTypeFlags(void *object, int field) const;
    virtual const char *getFieldTypeString(void *object, int field) const;
    virtual const char *getFieldProperty(void *object, int field, const char *propertyname) const;
    virtual int getArraySize(void *object, int field) const;

    virtual std::string getFieldAsString(void *object, int field, int i) const;
    virtual bool setFieldAsString(void *object, int field, int i, const char *value) const;

    virtual const char *getFieldStructName(void *object, int field) const;
    virtual void *getFieldStructPointer(void *object, int field, int i) const;
};

Register_ClassDescriptor(BPABPacketDescriptor);

BPABPacketDescriptor::BPABPacketDescriptor() : cClassDescriptor("BPABPacket", "MacPacket")
{
}

BPABPacketDescriptor::~BPABPacketDescriptor()
{
}

bool BPABPacketDescriptor::doesSupport(cObject *obj) const
{
    return dynamic_cast<BPABPacket *>(obj)!=NULL;
}

const char *BPABPacketDescriptor::getProperty(const char *propertyname) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : NULL;
}

int BPABPacketDescriptor::getFieldCount(void *object) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 13+basedesc->getFieldCount(object) : 13;
}

unsigned int BPABPacketDescriptor::getFieldTypeFlags(void *object, int field) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldTypeFlags(object, field);
        field -= basedesc->getFieldCount(object);
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
    };
    return (field>=0 && field<13) ? fieldTypeFlags[field] : 0;
}

const char *BPABPacketDescriptor::getFieldName(void *object, int field) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldName(object, field);
        field -= basedesc->getFieldCount(object);
    }
    static const char *fieldNames[] = {
        "bpabType",
        "direction",
        "rtbSentTime",
        "sourceId",
        "destinationId",
        "sourceX",
        "sourceY",
        "iteration",
        "maxIterations",
        "rangeR",
        "limitL",
        "limitU",
        "payload",
    };
    return (field>=0 && field<13) ? fieldNames[field] : NULL;
}

int BPABPacketDescriptor::findField(void *object, const char *fieldName) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount(object) : 0;
    if (fieldName[0]=='b' && strcmp(fieldName, "bpabType")==0) return base+0;
    if (fieldName[0]=='d' && strcmp(fieldName, "direction")==0) return base+1;
    if (fieldName[0]=='r' && strcmp(fieldName, "rtbSentTime")==0) return base+2;
    if (fieldName[0]=='s' && strcmp(fieldName, "sourceId")==0) return base+3;
    if (fieldName[0]=='d' && strcmp(fieldName, "destinationId")==0) return base+4;
    if (fieldName[0]=='s' && strcmp(fieldName, "sourceX")==0) return base+5;
    if (fieldName[0]=='s' && strcmp(fieldName, "sourceY")==0) return base+6;
    if (fieldName[0]=='i' && strcmp(fieldName, "iteration")==0) return base+7;
    if (fieldName[0]=='m' && strcmp(fieldName, "maxIterations")==0) return base+8;
    if (fieldName[0]=='r' && strcmp(fieldName, "rangeR")==0) return base+9;
    if (fieldName[0]=='l' && strcmp(fieldName, "limitL")==0) return base+10;
    if (fieldName[0]=='l' && strcmp(fieldName, "limitU")==0) return base+11;
    if (fieldName[0]=='p' && strcmp(fieldName, "payload")==0) return base+12;
    return basedesc ? basedesc->findField(object, fieldName) : -1;
}

const char *BPABPacketDescriptor::getFieldTypeString(void *object, int field) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldTypeString(object, field);
        field -= basedesc->getFieldCount(object);
    }
    static const char *fieldTypeStrings[] = {
        "int",
        "int",
        "double",
        "int",
        "int",
        "double",
        "double",
        "int",
        "int",
        "double",
        "double",
        "double",
        "string",
    };
    return (field>=0 && field<13) ? fieldTypeStrings[field] : NULL;
}

const char *BPABPacketDescriptor::getFieldProperty(void *object, int field, const char *propertyname) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldProperty(object, field, propertyname);
        field -= basedesc->getFieldCount(object);
    }
    switch (field) {
        case 0:
            if (!strcmp(propertyname,"enum")) return "BPABMessageType";
            return NULL;
        case 1:
            if (!strcmp(propertyname,"enum")) return "direction";
            return NULL;
        default: return NULL;
    }
}

int BPABPacketDescriptor::getArraySize(void *object, int field) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getArraySize(object, field);
        field -= basedesc->getFieldCount(object);
    }
    BPABPacket *pp = (BPABPacket *)object; (void)pp;
    switch (field) {
        default: return 0;
    }
}

std::string BPABPacketDescriptor::getFieldAsString(void *object, int field, int i) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldAsString(object,field,i);
        field -= basedesc->getFieldCount(object);
    }
    BPABPacket *pp = (BPABPacket *)object; (void)pp;
    switch (field) {
        case 0: return long2string(pp->getBpabType());
        case 1: return long2string(pp->getDirection());
        case 2: return double2string(pp->getRtbSentTime());
        case 3: return long2string(pp->getSourceId());
        case 4: return long2string(pp->getDestinationId());
        case 5: return double2string(pp->getSourceX());
        case 6: return double2string(pp->getSourceY());
        case 7: return long2string(pp->getIteration());
        case 8: return long2string(pp->getMaxIterations());
        case 9: return double2string(pp->getRangeR());
        case 10: return double2string(pp->getLimitL());
        case 11: return double2string(pp->getLimitU());
        case 12: return oppstring2string(pp->getPayload());
        default: return "";
    }
}

bool BPABPacketDescriptor::setFieldAsString(void *object, int field, int i, const char *value) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->setFieldAsString(object,field,i,value);
        field -= basedesc->getFieldCount(object);
    }
    BPABPacket *pp = (BPABPacket *)object; (void)pp;
    switch (field) {
        case 0: pp->setBpabType(string2long(value)); return true;
        case 1: pp->setDirection(string2long(value)); return true;
        case 2: pp->setRtbSentTime(string2double(value)); return true;
        case 3: pp->setSourceId(string2long(value)); return true;
        case 4: pp->setDestinationId(string2long(value)); return true;
        case 5: pp->setSourceX(string2double(value)); return true;
        case 6: pp->setSourceY(string2double(value)); return true;
        case 7: pp->setIteration(string2long(value)); return true;
        case 8: pp->setMaxIterations(string2long(value)); return true;
        case 9: pp->setRangeR(string2double(value)); return true;
        case 10: pp->setLimitL(string2double(value)); return true;
        case 11: pp->setLimitU(string2double(value)); return true;
        case 12: pp->setPayload((value)); return true;
        default: return false;
    }
}

const char *BPABPacketDescriptor::getFieldStructName(void *object, int field) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldStructName(object, field);
        field -= basedesc->getFieldCount(object);
    }
    switch (field) {
        default: return NULL;
    };
}

void *BPABPacketDescriptor::getFieldStructPointer(void *object, int field, int i) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldStructPointer(object, field, i);
        field -= basedesc->getFieldCount(object);
    }
    BPABPacket *pp = (BPABPacket *)object; (void)pp;
    switch (field) {
        default: return NULL;
    }
}


