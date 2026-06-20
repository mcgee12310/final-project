//
// Generated file, do not edit! Created by nedtool 4.6 from src/node/communication/mac/spMac/SpPacket.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#include <iostream>
#include <sstream>
#include "SpPacket_m.h"

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

Register_Class(SpPacket);

SpPacket::SpPacket(const char *name, int kind) : ::MacPacket(name,kind)
{
    this->spType_var = 1;
    this->originalSourceId_var = 0;
    this->senderId_var = 0;
    this->spSeqNumber_var = 0;
    this->creationTime_var = 0;
    this->sourceX_var = 0;
    this->sourceY_var = 0;
    this->senderX_var = 0;
    this->senderY_var = 0;
    this->hopCount_var = 0;
    this->ttl_var = 0;
    this->payload_var = 0;
}

SpPacket::SpPacket(const SpPacket& other) : ::MacPacket(other)
{
    copy(other);
}

SpPacket::~SpPacket()
{
}

SpPacket& SpPacket::operator=(const SpPacket& other)
{
    if (this==&other) return *this;
    ::MacPacket::operator=(other);
    copy(other);
    return *this;
}

void SpPacket::copy(const SpPacket& other)
{
    this->spType_var = other.spType_var;
    this->originalSourceId_var = other.originalSourceId_var;
    this->senderId_var = other.senderId_var;
    this->spSeqNumber_var = other.spSeqNumber_var;
    this->creationTime_var = other.creationTime_var;
    this->sourceX_var = other.sourceX_var;
    this->sourceY_var = other.sourceY_var;
    this->senderX_var = other.senderX_var;
    this->senderY_var = other.senderY_var;
    this->hopCount_var = other.hopCount_var;
    this->ttl_var = other.ttl_var;
    this->payload_var = other.payload_var;
}

void SpPacket::parsimPack(cCommBuffer *b)
{
    ::MacPacket::parsimPack(b);
    doPacking(b,this->spType_var);
    doPacking(b,this->originalSourceId_var);
    doPacking(b,this->senderId_var);
    doPacking(b,this->spSeqNumber_var);
    doPacking(b,this->creationTime_var);
    doPacking(b,this->sourceX_var);
    doPacking(b,this->sourceY_var);
    doPacking(b,this->senderX_var);
    doPacking(b,this->senderY_var);
    doPacking(b,this->hopCount_var);
    doPacking(b,this->ttl_var);
    doPacking(b,this->payload_var);
}

void SpPacket::parsimUnpack(cCommBuffer *b)
{
    ::MacPacket::parsimUnpack(b);
    doUnpacking(b,this->spType_var);
    doUnpacking(b,this->originalSourceId_var);
    doUnpacking(b,this->senderId_var);
    doUnpacking(b,this->spSeqNumber_var);
    doUnpacking(b,this->creationTime_var);
    doUnpacking(b,this->sourceX_var);
    doUnpacking(b,this->sourceY_var);
    doUnpacking(b,this->senderX_var);
    doUnpacking(b,this->senderY_var);
    doUnpacking(b,this->hopCount_var);
    doUnpacking(b,this->ttl_var);
    doUnpacking(b,this->payload_var);
}

int SpPacket::getSpType() const
{
    return spType_var;
}

void SpPacket::setSpType(int spType)
{
    this->spType_var = spType;
}

int SpPacket::getOriginalSourceId() const
{
    return originalSourceId_var;
}

void SpPacket::setOriginalSourceId(int originalSourceId)
{
    this->originalSourceId_var = originalSourceId;
}

int SpPacket::getSenderId() const
{
    return senderId_var;
}

void SpPacket::setSenderId(int senderId)
{
    this->senderId_var = senderId;
}

int SpPacket::getSpSeqNumber() const
{
    return spSeqNumber_var;
}

void SpPacket::setSpSeqNumber(int spSeqNumber)
{
    this->spSeqNumber_var = spSeqNumber;
}

double SpPacket::getCreationTime() const
{
    return creationTime_var;
}

void SpPacket::setCreationTime(double creationTime)
{
    this->creationTime_var = creationTime;
}

double SpPacket::getSourceX() const
{
    return sourceX_var;
}

void SpPacket::setSourceX(double sourceX)
{
    this->sourceX_var = sourceX;
}

double SpPacket::getSourceY() const
{
    return sourceY_var;
}

void SpPacket::setSourceY(double sourceY)
{
    this->sourceY_var = sourceY;
}

double SpPacket::getSenderX() const
{
    return senderX_var;
}

void SpPacket::setSenderX(double senderX)
{
    this->senderX_var = senderX;
}

double SpPacket::getSenderY() const
{
    return senderY_var;
}

void SpPacket::setSenderY(double senderY)
{
    this->senderY_var = senderY;
}

int SpPacket::getHopCount() const
{
    return hopCount_var;
}

void SpPacket::setHopCount(int hopCount)
{
    this->hopCount_var = hopCount;
}

int SpPacket::getTtl() const
{
    return ttl_var;
}

void SpPacket::setTtl(int ttl)
{
    this->ttl_var = ttl;
}

const char * SpPacket::getPayload() const
{
    return payload_var.c_str();
}

void SpPacket::setPayload(const char * payload)
{
    this->payload_var = payload;
}

class SpPacketDescriptor : public cClassDescriptor
{
  public:
    SpPacketDescriptor();
    virtual ~SpPacketDescriptor();

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

Register_ClassDescriptor(SpPacketDescriptor);

SpPacketDescriptor::SpPacketDescriptor() : cClassDescriptor("SpPacket", "MacPacket")
{
}

SpPacketDescriptor::~SpPacketDescriptor()
{
}

bool SpPacketDescriptor::doesSupport(cObject *obj) const
{
    return dynamic_cast<SpPacket *>(obj)!=NULL;
}

const char *SpPacketDescriptor::getProperty(const char *propertyname) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : NULL;
}

int SpPacketDescriptor::getFieldCount(void *object) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 12+basedesc->getFieldCount(object) : 12;
}

unsigned int SpPacketDescriptor::getFieldTypeFlags(void *object, int field) const
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
    };
    return (field>=0 && field<12) ? fieldTypeFlags[field] : 0;
}

const char *SpPacketDescriptor::getFieldName(void *object, int field) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldName(object, field);
        field -= basedesc->getFieldCount(object);
    }
    static const char *fieldNames[] = {
        "spType",
        "originalSourceId",
        "senderId",
        "spSeqNumber",
        "creationTime",
        "sourceX",
        "sourceY",
        "senderX",
        "senderY",
        "hopCount",
        "ttl",
        "payload",
    };
    return (field>=0 && field<12) ? fieldNames[field] : NULL;
}

int SpPacketDescriptor::findField(void *object, const char *fieldName) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount(object) : 0;
    if (fieldName[0]=='s' && strcmp(fieldName, "spType")==0) return base+0;
    if (fieldName[0]=='o' && strcmp(fieldName, "originalSourceId")==0) return base+1;
    if (fieldName[0]=='s' && strcmp(fieldName, "senderId")==0) return base+2;
    if (fieldName[0]=='s' && strcmp(fieldName, "spSeqNumber")==0) return base+3;
    if (fieldName[0]=='c' && strcmp(fieldName, "creationTime")==0) return base+4;
    if (fieldName[0]=='s' && strcmp(fieldName, "sourceX")==0) return base+5;
    if (fieldName[0]=='s' && strcmp(fieldName, "sourceY")==0) return base+6;
    if (fieldName[0]=='s' && strcmp(fieldName, "senderX")==0) return base+7;
    if (fieldName[0]=='s' && strcmp(fieldName, "senderY")==0) return base+8;
    if (fieldName[0]=='h' && strcmp(fieldName, "hopCount")==0) return base+9;
    if (fieldName[0]=='t' && strcmp(fieldName, "ttl")==0) return base+10;
    if (fieldName[0]=='p' && strcmp(fieldName, "payload")==0) return base+11;
    return basedesc ? basedesc->findField(object, fieldName) : -1;
}

const char *SpPacketDescriptor::getFieldTypeString(void *object, int field) const
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
        "int",
        "int",
        "double",
        "double",
        "double",
        "double",
        "double",
        "int",
        "int",
        "string",
    };
    return (field>=0 && field<12) ? fieldTypeStrings[field] : NULL;
}

const char *SpPacketDescriptor::getFieldProperty(void *object, int field, const char *propertyname) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldProperty(object, field, propertyname);
        field -= basedesc->getFieldCount(object);
    }
    switch (field) {
        default: return NULL;
    }
}

int SpPacketDescriptor::getArraySize(void *object, int field) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getArraySize(object, field);
        field -= basedesc->getFieldCount(object);
    }
    SpPacket *pp = (SpPacket *)object; (void)pp;
    switch (field) {
        default: return 0;
    }
}

std::string SpPacketDescriptor::getFieldAsString(void *object, int field, int i) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldAsString(object,field,i);
        field -= basedesc->getFieldCount(object);
    }
    SpPacket *pp = (SpPacket *)object; (void)pp;
    switch (field) {
        case 0: return long2string(pp->getSpType());
        case 1: return long2string(pp->getOriginalSourceId());
        case 2: return long2string(pp->getSenderId());
        case 3: return long2string(pp->getSpSeqNumber());
        case 4: return double2string(pp->getCreationTime());
        case 5: return double2string(pp->getSourceX());
        case 6: return double2string(pp->getSourceY());
        case 7: return double2string(pp->getSenderX());
        case 8: return double2string(pp->getSenderY());
        case 9: return long2string(pp->getHopCount());
        case 10: return long2string(pp->getTtl());
        case 11: return oppstring2string(pp->getPayload());
        default: return "";
    }
}

bool SpPacketDescriptor::setFieldAsString(void *object, int field, int i, const char *value) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->setFieldAsString(object,field,i,value);
        field -= basedesc->getFieldCount(object);
    }
    SpPacket *pp = (SpPacket *)object; (void)pp;
    switch (field) {
        case 0: pp->setSpType(string2long(value)); return true;
        case 1: pp->setOriginalSourceId(string2long(value)); return true;
        case 2: pp->setSenderId(string2long(value)); return true;
        case 3: pp->setSpSeqNumber(string2long(value)); return true;
        case 4: pp->setCreationTime(string2double(value)); return true;
        case 5: pp->setSourceX(string2double(value)); return true;
        case 6: pp->setSourceY(string2double(value)); return true;
        case 7: pp->setSenderX(string2double(value)); return true;
        case 8: pp->setSenderY(string2double(value)); return true;
        case 9: pp->setHopCount(string2long(value)); return true;
        case 10: pp->setTtl(string2long(value)); return true;
        case 11: pp->setPayload((value)); return true;
        default: return false;
    }
}

const char *SpPacketDescriptor::getFieldStructName(void *object, int field) const
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

void *SpPacketDescriptor::getFieldStructPointer(void *object, int field, int i) const
{
    cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount(object))
            return basedesc->getFieldStructPointer(object, field, i);
        field -= basedesc->getFieldCount(object);
    }
    SpPacket *pp = (SpPacket *)object; (void)pp;
    switch (field) {
        default: return NULL;
    }
}


