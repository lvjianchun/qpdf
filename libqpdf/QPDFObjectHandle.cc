#include <qpdf/QPDFObjectHandle.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDF_Bool.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Integer.hh>
#include <qpdf/QPDF_Real.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QPDF_String.hh>
#include <qpdf/QPDF_Operator.hh>
#include <qpdf/QPDF_InlineImage.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_Stream.hh>
#include <qpdf/QPDF_Reserved.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Concatenate.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/QPDFExc.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <stdexcept>
#include <stdlib.h>
#include <ctype.h>

class TerminateParsing
{
};

class CoalesceProvider: public QPDFObjectHandle::StreamDataProvider
{
  public:
    CoalesceProvider(QPDFObjectHandle containing_page,
                     QPDFObjectHandle old_contents) :
        containing_page(containing_page),
        old_contents(old_contents)
    {
    }
    virtual ~CoalesceProvider()
    {
    }
    virtual void provideStreamData(int objid, int generation,
                                   Pipeline* pipeline);

  private:
    QPDFObjectHandle containing_page;
    QPDFObjectHandle old_contents;
};

void
CoalesceProvider::provideStreamData(int, int, Pipeline* p)
{
    QTC::TC("qpdf", "QPDFObjectHandle coalesce provide stream data");
    Pl_Concatenate concat("concatenate", p);
    std::string description = "page object " +
        QUtil::int_to_string(containing_page.getObjectID()) + " " +
        QUtil::int_to_string(containing_page.getGeneration());
    std::string all_description;
    old_contents.pipeContentStreams(&concat, description, all_description);
    concat.manualFinish();
}

void
QPDFObjectHandle::TokenFilter::handleEOF()
{
}

void
QPDFObjectHandle::TokenFilter::setPipeline(Pipeline* p)
{
    this->pipeline = p;
}

void
QPDFObjectHandle::TokenFilter::write(char const* data, size_t len)
{
    if (! this->pipeline)
    {
        return;
    }
    if (len)
    {
	this->pipeline->write(QUtil::unsigned_char_pointer(data), len);
    }
}

void
QPDFObjectHandle::TokenFilter::write(std::string const& str)
{
    write(str.c_str(), str.length());
}

void
QPDFObjectHandle::TokenFilter::writeToken(QPDFTokenizer::Token const& token)
{
    std::string value = token.getRawValue();
    write(value.c_str(), value.length());
}

void
QPDFObjectHandle::ParserCallbacks::terminateParsing()
{
    throw TerminateParsing();
}

QPDFObjectHandle::QPDFObjectHandle() :
    initialized(false),
    qpdf(0),
    objid(0),
    generation(0),
    reserved(false)
{
}

QPDFObjectHandle::QPDFObjectHandle(QPDF* qpdf, int objid, int generation) :
    initialized(true),
    qpdf(qpdf),
    objid(objid),
    generation(generation),
    reserved(false)
{
}

QPDFObjectHandle::QPDFObjectHandle(QPDFObject* data) :
    initialized(true),
    qpdf(0),
    objid(0),
    generation(0),
    obj(data),
    reserved(false)
{
}

void
QPDFObjectHandle::releaseResolved()
{
    // Recursively break any resolved references to indirect objects.
    // Do not cross over indirect object boundaries to avoid an
    // infinite loop.  This method may only be called during final
    // destruction.  See comments in QPDF::~QPDF().
    if (isIndirect())
    {
	if (this->obj.getPointer())
	{
	    this->obj = 0;
	}
    }
    else
    {
	QPDFObject::ObjAccessor::releaseResolved(this->obj.getPointer());
    }
}

bool
QPDFObjectHandle::isInitialized() const
{
    return this->initialized;
}

QPDFObject::object_type_e
QPDFObjectHandle::getTypeCode()
{
    if (this->initialized)
    {
        dereference();
        return obj->getTypeCode();
    }
    else
    {
        return QPDFObject::ot_uninitialized;
    }
}

char const*
QPDFObjectHandle::getTypeName()
{
    if (this->initialized)
    {
        dereference();
        return obj->getTypeName();
    }
    else
    {
        return "uninitialized";
    }
}

template <class T>
class QPDFObjectTypeAccessor
{
  public:
    static bool check(QPDFObject* o)
    {
	return (o && dynamic_cast<T*>(o));
    }
};

bool
QPDFObjectHandle::isBool()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Bool>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isNull()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Null>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isInteger()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Integer>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isReal()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Real>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isNumber()
{
    return (isInteger() || isReal());
}

double
QPDFObjectHandle::getNumericValue()
{
    double result = 0.0;
    if (isInteger())
    {
	result = static_cast<double>(getIntValue());
    }
    else if (isReal())
    {
	result = atof(getRealValue().c_str());
    }
    else
    {
	throw std::logic_error("getNumericValue called for non-numeric object");
    }
    return result;
}

bool
QPDFObjectHandle::isName()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Name>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isString()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_String>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isOperator()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Operator>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isInlineImage()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_InlineImage>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isArray()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Array>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isDictionary()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Dictionary>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isStream()
{
    dereference();
    return QPDFObjectTypeAccessor<QPDF_Stream>::check(obj.getPointer());
}

bool
QPDFObjectHandle::isReserved()
{
    // dereference will clear reserved if this has been replaced
    dereference();
    return this->reserved;
}

bool
QPDFObjectHandle::isIndirect()
{
    assertInitialized();
    return (this->objid != 0);
}

bool
QPDFObjectHandle::isScalar()
{
    return (! (isArray() || isDictionary() || isStream() ||
               isOperator() || isInlineImage()));
}

// Bool accessors

bool
QPDFObjectHandle::getBoolValue()
{
    assertBool();
    return dynamic_cast<QPDF_Bool*>(obj.getPointer())->getVal();
}

// Integer accessors

long long
QPDFObjectHandle::getIntValue()
{
    assertInteger();
    return dynamic_cast<QPDF_Integer*>(obj.getPointer())->getVal();
}

// Real accessors

std::string
QPDFObjectHandle::getRealValue()
{
    assertReal();
    return dynamic_cast<QPDF_Real*>(obj.getPointer())->getVal();
}

// Name accessors

std::string
QPDFObjectHandle::getName()
{
    assertName();
    return dynamic_cast<QPDF_Name*>(obj.getPointer())->getName();
}

// String accessors

std::string
QPDFObjectHandle::getStringValue()
{
    assertString();
    return dynamic_cast<QPDF_String*>(obj.getPointer())->getVal();
}

std::string
QPDFObjectHandle::getUTF8Value()
{
    assertString();
    return dynamic_cast<QPDF_String*>(obj.getPointer())->getUTF8Val();
}

// Operator and Inline Image accessors

std::string
QPDFObjectHandle::getOperatorValue()
{
    assertOperator();
    return dynamic_cast<QPDF_Operator*>(obj.getPointer())->getVal();
}

std::string
QPDFObjectHandle::getInlineImageValue()
{
    assertInlineImage();
    return dynamic_cast<QPDF_InlineImage*>(obj.getPointer())->getVal();
}

// Array accessors

int
QPDFObjectHandle::getArrayNItems()
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->getNItems();
}

QPDFObjectHandle
QPDFObjectHandle::getArrayItem(int n)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->getItem(n);
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getArrayAsVector()
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->getAsVector();
}

// Array mutators

void
QPDFObjectHandle::setArrayItem(int n, QPDFObjectHandle const& item)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->setItem(n, item);
}

void
QPDFObjectHandle::setArrayFromVector(std::vector<QPDFObjectHandle> const& items)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->setFromVector(items);
}

void
QPDFObjectHandle::insertItem(int at, QPDFObjectHandle const& item)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->insertItem(at, item);
}

void
QPDFObjectHandle::appendItem(QPDFObjectHandle const& item)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->appendItem(item);
}

void
QPDFObjectHandle::eraseItem(int at)
{
    assertArray();
    return dynamic_cast<QPDF_Array*>(obj.getPointer())->eraseItem(at);
}

// Dictionary accessors

bool
QPDFObjectHandle::hasKey(std::string const& key)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->hasKey(key);
}

QPDFObjectHandle
QPDFObjectHandle::getKey(std::string const& key)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->getKey(key);
}

std::set<std::string>
QPDFObjectHandle::getKeys()
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->getKeys();
}

std::map<std::string, QPDFObjectHandle>
QPDFObjectHandle::getDictAsMap()
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->getAsMap();
}

// Array and Name accessors
bool
QPDFObjectHandle::isOrHasName(std::string const& value)
{
    if (isName() && (getName() == value))
    {
	return true;
    }
    else if (isArray())
    {
	int n = getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    QPDFObjectHandle item = getArrayItem(0);
	    if (item.isName() && (item.getName() == value))
	    {
		return true;
	    }
	}
    }
    return false;
}

// Indirect object accessors
QPDF*
QPDFObjectHandle::getOwningQPDF()
{
    // Will be null for direct objects
    return this->qpdf;
}

// Dictionary mutators

void
QPDFObjectHandle::replaceKey(std::string const& key,
			    QPDFObjectHandle const& value)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(
	obj.getPointer())->replaceKey(key, value);
}

void
QPDFObjectHandle::removeKey(std::string const& key)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(obj.getPointer())->removeKey(key);
}

void
QPDFObjectHandle::replaceOrRemoveKey(std::string const& key,
				     QPDFObjectHandle value)
{
    assertDictionary();
    return dynamic_cast<QPDF_Dictionary*>(
	obj.getPointer())->replaceOrRemoveKey(key, value);
}

// Stream accessors
QPDFObjectHandle
QPDFObjectHandle::getDict()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getDict();
}

bool
QPDFObjectHandle::isDataModified()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->isDataModified();
}

void
QPDFObjectHandle::replaceDict(QPDFObjectHandle new_dict)
{
    assertStream();
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceDict(new_dict);
}

PointerHolder<Buffer>
QPDFObjectHandle::getStreamData(qpdf_stream_decode_level_e level)
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getStreamData(level);
}

PointerHolder<Buffer>
QPDFObjectHandle::getRawStreamData()
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->getRawStreamData();
}

bool
QPDFObjectHandle::pipeStreamData(Pipeline* p,
                                 unsigned long encode_flags,
                                 qpdf_stream_decode_level_e decode_level,
                                 bool suppress_warnings, bool will_retry)
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(obj.getPointer())->pipeStreamData(
	p, encode_flags, decode_level, suppress_warnings, will_retry);
}

bool
QPDFObjectHandle::pipeStreamData(Pipeline* p, bool filter,
				 bool normalize, bool compress)
{
    unsigned long encode_flags = 0;
    qpdf_stream_decode_level_e decode_level = qpdf_dl_none;
    if (filter)
    {
        decode_level = qpdf_dl_generalized;
        if (normalize)
        {
            encode_flags |= qpdf_ef_normalize;
        }
        if (compress)
        {
            encode_flags |= qpdf_ef_compress;
        }
    }
    return pipeStreamData(p, encode_flags, decode_level, false);
}

void
QPDFObjectHandle::replaceStreamData(PointerHolder<Buffer> data,
				    QPDFObjectHandle const& filter,
				    QPDFObjectHandle const& decode_parms)
{
    assertStream();
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceStreamData(
	data, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(std::string const& data,
				    QPDFObjectHandle const& filter,
				    QPDFObjectHandle const& decode_parms)
{
    assertStream();
    PointerHolder<Buffer> b = new Buffer(data.length());
    unsigned char* bp = b->getBuffer();
    memcpy(bp, data.c_str(), data.length());
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceStreamData(
	b, filter, decode_parms);
}

void
QPDFObjectHandle::replaceStreamData(PointerHolder<StreamDataProvider> provider,
				    QPDFObjectHandle const& filter,
				    QPDFObjectHandle const& decode_parms)
{
    assertStream();
    dynamic_cast<QPDF_Stream*>(obj.getPointer())->replaceStreamData(
	provider, filter, decode_parms);
}

QPDFObjGen
QPDFObjectHandle::getObjGen() const
{
    return QPDFObjGen(this->objid, this->generation);
}

int
QPDFObjectHandle::getObjectID() const
{
    return this->objid;
}

int
QPDFObjectHandle::getGeneration() const
{
    return this->generation;
}

std::map<std::string, QPDFObjectHandle>
QPDFObjectHandle::getPageImages()
{
    assertPageObject();

    // Note: this code doesn't handle inherited resources.  If this
    // page dictionary doesn't have a /Resources key or has one whose
    // value is null or an empty dictionary, you are supposed to walk
    // up the page tree until you find a /Resources dictionary.  As of
    // this writing, I don't have any test files that use inherited
    // resources, and hand-generating one won't be a good test because
    // any mistakes in my understanding would be present in both the
    // code and the test file.

    // NOTE: If support of inherited resources (see above comment) is
    // implemented, edit comment in QPDFObjectHandle.hh for this
    // function.  Also remove call to pushInheritedAttributesToPage
    // from qpdf.cc when show_page_images is true.

    std::map<std::string, QPDFObjectHandle> result;
    if (this->hasKey("/Resources"))
    {
	QPDFObjectHandle resources = this->getKey("/Resources");
	if (resources.hasKey("/XObject"))
	{
	    QPDFObjectHandle xobject = resources.getKey("/XObject");
	    std::set<std::string> keys = xobject.getKeys();
	    for (std::set<std::string>::iterator iter = keys.begin();
		 iter != keys.end(); ++iter)
	    {
		std::string key = (*iter);
		QPDFObjectHandle value = xobject.getKey(key);
		if (value.isStream())
		{
		    QPDFObjectHandle dict = value.getDict();
		    if (dict.hasKey("/Subtype") &&
			(dict.getKey("/Subtype").getName() == "/Image") &&
			(! dict.hasKey("/ImageMask")))
		    {
			result[key] = value;
		    }
		}
	    }
	}
    }

    return result;
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::arrayOrStreamToStreamArray(
    std::string const& description, std::string& all_description)
{
    all_description = description;
    std::vector<QPDFObjectHandle> result;
    if (isArray())
    {
	int n_items = getArrayNItems();
	for (int i = 0; i < n_items; ++i)
	{
	    QPDFObjectHandle item = getArrayItem(i);
	    if (item.isStream())
            {
                result.push_back(item);
            }
            else
	    {
                QTC::TC("qpdf", "QPDFObjectHandle non-stream in stream array");
                warn(item.getOwningQPDF(),
                     QPDFExc(qpdf_e_damaged_pdf, description,
                             "item index " + QUtil::int_to_string(i) +
                             " (from 0)", 0,
                             "ignoring non-stream in an array of streams"));
	    }
	}
    }
    else if (isStream())
    {
	result.push_back(*this);
    }
    else if (! isNull())
    {
        warn(getOwningQPDF(),
             QPDFExc(qpdf_e_damaged_pdf, "", description, 0,
                     " object is supposed to be a stream or an"
                     " array of streams but is neither"));
    }

    bool first = true;
    for (std::vector<QPDFObjectHandle>::iterator iter = result.begin();
         iter != result.end(); ++iter)
    {
        QPDFObjectHandle item = *iter;
        std::string og =
            QUtil::int_to_string(item.getObjectID()) + " " +
            QUtil::int_to_string(item.getGeneration());
        if (first)
        {
            first = false;
        }
        else
        {
            all_description += ",";
        }
        all_description += " stream " + og;
    }

    return result;
}

std::vector<QPDFObjectHandle>
QPDFObjectHandle::getPageContents()
{
    std::string description = "page object " +
        QUtil::int_to_string(this->objid) + " " +
        QUtil::int_to_string(this->generation);
    std::string all_description;
    return this->getKey("/Contents").arrayOrStreamToStreamArray(
        description, all_description);
}

void
QPDFObjectHandle::addPageContents(QPDFObjectHandle new_contents, bool first)
{
    new_contents.assertStream();

    std::vector<QPDFObjectHandle> orig_contents = getPageContents();

    std::vector<QPDFObjectHandle> content_streams;
    if (first)
    {
	QTC::TC("qpdf", "QPDFObjectHandle prepend page contents");
	content_streams.push_back(new_contents);
    }
    for (std::vector<QPDFObjectHandle>::iterator iter = orig_contents.begin();
	 iter != orig_contents.end(); ++iter)
    {
	QTC::TC("qpdf", "QPDFObjectHandle append page contents");
	content_streams.push_back(*iter);
    }
    if (! first)
    {
	content_streams.push_back(new_contents);
    }

    QPDFObjectHandle contents = QPDFObjectHandle::newArray(content_streams);
    this->replaceKey("/Contents", contents);
}

void
QPDFObjectHandle::rotatePage(int angle, bool relative)
{
    assertPageObject();
    if ((angle % 90) != 0)
    {
        throw std::runtime_error(
            "QPDF::rotatePage called with an"
            " angle that is not a multiple of 90");
    }
    int new_angle = angle;
    if (relative)
    {
        int old_angle = 0;
        bool found_rotate = false;
        QPDFObjectHandle cur_obj = *this;
        bool searched_parent = false;
        std::set<QPDFObjGen> visited;
        while (! found_rotate)
        {
            if (visited.count(cur_obj.getObjGen()))
            {
                // Don't get stuck in an infinite loop
                break;
            }
            if (! visited.empty())
            {
                searched_parent = true;
            }
            visited.insert(cur_obj.getObjGen());
            if (cur_obj.getKey("/Rotate").isInteger())
            {
                found_rotate = true;
                old_angle = cur_obj.getKey("/Rotate").getIntValue();
            }
            else if (cur_obj.getKey("/Parent").isDictionary())
            {
                cur_obj = cur_obj.getKey("/Parent");
            }
            else
            {
                break;
            }
        }
        QTC::TC("qpdf", "QPDFObjectHandle found old angle",
                searched_parent ? 0 : 1);
        if ((old_angle % 90) != 0)
        {
            old_angle = 0;
        }
        new_angle += old_angle;
    }
    new_angle = (new_angle + 360) % 360;
    replaceKey("/Rotate", QPDFObjectHandle::newInteger(new_angle));
}

void
QPDFObjectHandle::coalesceContentStreams()
{
    assertPageObject();
    QPDFObjectHandle contents = this->getKey("/Contents");
    if (contents.isStream())
    {
        QTC::TC("qpdf", "QPDFObjectHandle coalesce called on stream");
        return;
    }
    QPDF* qpdf = getOwningQPDF();
    if (qpdf == 0)
    {
        // Should not be possible for a page object to not have an
        // owning PDF unless it was manually constructed in some
        // incorrect way.
        throw std::logic_error("coalesceContentStreams called on object"
                               " with no associated PDF file");
    }
    QPDFObjectHandle new_contents = newStream(qpdf);
    this->replaceKey("/Contents", new_contents);

    PointerHolder<StreamDataProvider> provider =
        new CoalesceProvider(*this, contents);
    new_contents.replaceStreamData(provider, newNull(), newNull());
}

std::string
QPDFObjectHandle::unparse()
{
    std::string result;
    if (this->isIndirect())
    {
	result = QUtil::int_to_string(this->objid) + " " +
	    QUtil::int_to_string(this->generation) + " R";
    }
    else
    {
	result = unparseResolved();
    }
    return result;
}

std::string
QPDFObjectHandle::unparseResolved()
{
    if (this->reserved)
    {
        throw std::logic_error(
            "QPDFObjectHandle: attempting to unparse a reserved object");
    }
    dereference();
    return this->obj->unparse();
}

QPDFObjectHandle
QPDFObjectHandle::parse(std::string const& object_str,
                        std::string const& object_description)
{
    PointerHolder<InputSource> input =
        new BufferInputSource("parsed object", object_str);
    QPDFTokenizer tokenizer;
    bool empty = false;
    QPDFObjectHandle result =
        parse(input, object_description, tokenizer, empty, 0, 0);
    size_t offset = input->tell();
    while (offset < object_str.length())
    {
        if (! isspace(object_str.at(offset)))
        {
            QTC::TC("qpdf", "QPDFObjectHandle trailing data in parse");
            throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                          object_description,
                          input->getLastOffset(),
                          "trailing data found parsing object from string");
        }
        ++offset;
    }
    return result;
}

void
QPDFObjectHandle::pipePageContents(Pipeline* p)
{
    assertPageObject();
    std::string description = "page object " +
        QUtil::int_to_string(this->objid) + " " +
        QUtil::int_to_string(this->generation);
    std::string all_description;
    this->getKey("/Contents").pipeContentStreams(
        p, description, all_description);
}

void
QPDFObjectHandle::pipeContentStreams(
    Pipeline* p, std::string const& description, std::string& all_description)
{
    std::vector<QPDFObjectHandle> streams =
        arrayOrStreamToStreamArray(
            description, all_description);
    for (std::vector<QPDFObjectHandle>::iterator iter = streams.begin();
         iter != streams.end(); ++iter)
    {
        QPDFObjectHandle stream = *iter;
        std::string og =
            QUtil::int_to_string(stream.getObjectID()) + " " +
            QUtil::int_to_string(stream.getGeneration());
        std::string description = "content stream object " + og;
        if (! stream.pipeStreamData(p, 0, qpdf_dl_specialized))
        {
            QTC::TC("qpdf", "QPDFObjectHandle errors in parsecontent");
            warn(stream.getOwningQPDF(),
                 QPDFExc(qpdf_e_damaged_pdf, "content stream",
                         description, 0,
                         "errors while decoding content stream"));
        }
    }
}

void
QPDFObjectHandle::parsePageContents(ParserCallbacks* callbacks)
{
    assertPageObject();
    std::string description = "page object " +
        QUtil::int_to_string(this->objid) + " " +
        QUtil::int_to_string(this->generation);
    this->getKey("/Contents").parseContentStream_internal(
        description, callbacks);
}

void
QPDFObjectHandle::filterPageContents(TokenFilter* filter, Pipeline* next)
{
    assertPageObject();
    std::string description = "token filter for page object " +
        QUtil::int_to_string(this->objid) + " " +
        QUtil::int_to_string(this->generation);
    Pl_QPDFTokenizer token_pipeline(description.c_str(), filter, next);
    this->pipePageContents(&token_pipeline);
}

void
QPDFObjectHandle::parseContentStream(QPDFObjectHandle stream_or_array,
                                     ParserCallbacks* callbacks)
{
    stream_or_array.parseContentStream_internal(
        "content stream objects", callbacks);
}

void
QPDFObjectHandle::parseContentStream_internal(
    std::string const& description,
    ParserCallbacks* callbacks)
{
    Pl_Buffer buf("concatenated stream data buffer");
    std::string all_description;
    pipeContentStreams(&buf, description, all_description);
    PointerHolder<Buffer> stream_data = buf.getBuffer();
    try
    {
        parseContentStream_data(stream_data, all_description, callbacks);
    }
    catch (TerminateParsing&)
    {
        return;
    }
    callbacks->handleEOF();
}

void
QPDFObjectHandle::parseContentStream_data(
    PointerHolder<Buffer> stream_data,
    std::string const& description,
    ParserCallbacks* callbacks)
{
    size_t length = stream_data->getSize();
    PointerHolder<InputSource> input =
        new BufferInputSource(description, stream_data.getPointer());
    QPDFTokenizer tokenizer;
    tokenizer.allowEOF();
    bool empty = false;
    while (static_cast<size_t>(input->tell()) < length)
    {
        QPDFObjectHandle obj =
            parseInternal(input, "content", tokenizer, empty, 0, 0, true);
        if (! obj.isInitialized())
        {
            // EOF
            break;
        }

        callbacks->handleObject(obj);
        if (obj.isOperator() && (obj.getOperatorValue() == "ID"))
        {
            // Discard next character; it is the space after ID that
            // terminated the token.  Read until end of inline image.
            char ch;
            input->read(&ch, 1);
            tokenizer.expectInlineImage();
            QPDFTokenizer::Token t = tokenizer.readToken(input, description, true);
            if (t.getType() == QPDFTokenizer::tt_bad)
            {
                QTC::TC("qpdf", "QPDFObjectHandle EOF in inline image");
                throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                              "stream data", input->tell(),
                              "EOF found while reading inline image");
            }
            else
            {
                // Skip back over EI
                input->seek(-3, SEEK_CUR);
                std::string inline_image = t.getRawValue();
                for (int i = 0; i < 4; ++i)
                {
                    if (inline_image.length() > 0)
                    {
                        inline_image.erase(inline_image.length() - 1);
                    }
                }
                QTC::TC("qpdf", "QPDFObjectHandle inline image token");
                callbacks->handleObject(
                    QPDFObjectHandle::newInlineImage(inline_image));
            }
        }
    }
}

void
QPDFObjectHandle::addContentTokenFilter(PointerHolder<TokenFilter> filter)
{
    coalesceContentStreams();
    this->getKey("/Contents").addTokenFilter(filter);
}

void
QPDFObjectHandle::addTokenFilter(PointerHolder<TokenFilter> filter)
{
    assertStream();
    return dynamic_cast<QPDF_Stream*>(
        obj.getPointer())->addTokenFilter(filter);
}

QPDFObjectHandle
QPDFObjectHandle::parse(PointerHolder<InputSource> input,
                        std::string const& object_description,
                        QPDFTokenizer& tokenizer, bool& empty,
                        StringDecrypter* decrypter, QPDF* context)
{
    return parseInternal(input, object_description, tokenizer, empty,
                         decrypter, context, false);
}

QPDFObjectHandle
QPDFObjectHandle::parseInternal(PointerHolder<InputSource> input,
                                std::string const& object_description,
                                QPDFTokenizer& tokenizer, bool& empty,
                                StringDecrypter* decrypter, QPDF* context,
                                bool content_stream)
{
    // This method must take care not to resolve any objects. Don't
    // check the type of any object without first ensuring that it is
    // a direct object. Otherwise, doing so may have the side effect
    // of reading the object and changing the file pointer.

    empty = false;

    QPDFObjectHandle object;

    std::vector<std::vector<QPDFObjectHandle> > olist_stack;
    olist_stack.push_back(std::vector<QPDFObjectHandle>());
    std::vector<parser_state_e> state_stack;
    state_stack.push_back(st_top);
    std::vector<qpdf_offset_t> offset_stack;
    offset_stack.push_back(input->tell());
    bool done = false;
    while (! done)
    {
        std::vector<QPDFObjectHandle>& olist = olist_stack.back();
        parser_state_e state = state_stack.back();
        qpdf_offset_t offset = offset_stack.back();

	object = QPDFObjectHandle();

	QPDFTokenizer::Token token =
            tokenizer.readToken(input, object_description);

	switch (token.getType())
	{
          case QPDFTokenizer::tt_eof:
            if (content_stream)
            {
                state = st_eof;
            }
            else
            {
                // When not in content stream mode, EOF is tt_bad and
                // throws an exception before we get here.
                throw std::logic_error(
                    "EOF received while not in content stream mode");
            }
            break;

	  case QPDFTokenizer::tt_brace_open:
	  case QPDFTokenizer::tt_brace_close:
	    QTC::TC("qpdf", "QPDFObjectHandle bad brace");
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         "treating unexpected brace token as null"));
            object = newNull();
	    break;

	  case QPDFTokenizer::tt_array_close:
	    if (state == st_array)
	    {
                state = st_stop;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDFObjectHandle bad array close");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "treating unexpected array close token as null"));
                object = newNull();
	    }
	    break;

	  case QPDFTokenizer::tt_dict_close:
	    if (state == st_dictionary)
	    {
                state = st_stop;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDFObjectHandle bad dictionary close");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "unexpected dictionary close token"));
                object = newNull();
	    }
	    break;

	  case QPDFTokenizer::tt_array_open:
	  case QPDFTokenizer::tt_dict_open:
            olist_stack.push_back(std::vector<QPDFObjectHandle>());
            state = st_start;
            offset_stack.push_back(input->tell());
            state_stack.push_back(
                (token.getType() == QPDFTokenizer::tt_array_open) ?
                st_array : st_dictionary);
	    break;

	  case QPDFTokenizer::tt_bool:
	    object = newBool((token.getValue() == "true"));
	    break;

	  case QPDFTokenizer::tt_null:
	    object = newNull();
	    break;

	  case QPDFTokenizer::tt_integer:
	    object = newInteger(QUtil::string_to_ll(token.getValue().c_str()));
	    break;

	  case QPDFTokenizer::tt_real:
	    object = newReal(token.getValue());
	    break;

	  case QPDFTokenizer::tt_name:
	    object = newName(token.getValue());
	    break;

	  case QPDFTokenizer::tt_word:
	    {
		std::string const& value = token.getValue();
                if (content_stream)
                {
                    object = QPDFObjectHandle::newOperator(value);
                }
		else if ((value == "R") && (state != st_top) &&
                         (olist.size() >= 2) &&
                         (! olist.at(olist.size() - 1).isIndirect()) &&
                         (olist.at(olist.size() - 1).isInteger()) &&
                         (! olist.at(olist.size() - 2).isIndirect()) &&
                         (olist.at(olist.size() - 2).isInteger()))
		{
                    if (context == 0)
                    {
                        QTC::TC("qpdf", "QPDFObjectHandle indirect without context");
                        throw std::logic_error(
                            "QPDFObjectHandle::parse called without context"
                            " on an object with indirect references");
                    }
		    // Try to resolve indirect objects
		    object = newIndirect(
			context,
			olist.at(olist.size() - 2).getIntValue(),
			olist.at(olist.size() - 1).getIntValue());
		    olist.pop_back();
		    olist.pop_back();
		}
		else if ((value == "endobj") && (state == st_top))
		{
		    // We just saw endobj without having read
		    // anything.  Treat this as a null and do not move
		    // the input source's offset.
		    object = newNull();
		    input->seek(input->getLastOffset(), SEEK_SET);
                    empty = true;
		}
		else
		{
                    QTC::TC("qpdf", "QPDFObjectHandle treat word as string");
                    warn(context,
                         QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                 object_description,
                                 input->getLastOffset(),
                                 "unknown token while reading object;"
                                 " treating as string"));
                    object = newString(value);
		}
	    }
	    break;

	  case QPDFTokenizer::tt_string:
	    {
		std::string val = token.getValue();
                if (decrypter)
                {
                    decrypter->decryptString(val);
                }
		object = QPDFObjectHandle::newString(val);
	    }

	    break;

	  default:
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         "treating unknown token type as null while "
                         "reading object"));
            object = newNull();
	    break;
	}

        if ((! object.isInitialized()) &&
            (! ((state == st_start) ||
                (state == st_stop) ||
                (state == st_eof))))
        {
            throw std::logic_error(
                "QPDFObjectHandle::parseInternal: "
                "unexpected uninitialized object");
            object = newNull();
        }

        switch (state)
        {
          case st_eof:
            if (state_stack.size() > 1)
            {
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "parse error while reading object"));
            }
            done = true;
            // Leave object uninitialized to indicate EOF
            break;

          case st_dictionary:
          case st_array:
            olist.push_back(object);
            break;

          case st_top:
            done = true;
            break;

          case st_start:
            break;

          case st_stop:
            if ((state_stack.size() < 2) || (olist_stack.size() < 2))
            {
                throw std::logic_error(
                    "QPDFObjectHandle::parseInternal: st_stop encountered"
                    " with insufficient elements in stack");
            }
            parser_state_e old_state = state_stack.back();
            state_stack.pop_back();
            if (old_state == st_array)
            {
                object = newArray(olist);
            }
            else if (old_state == st_dictionary)
            {
                // Convert list to map. Alternating elements are keys.
                // Attempt to recover more or less gracefully from
                // invalid dictionaries.
                std::set<std::string> names;
                for (std::vector<QPDFObjectHandle>::iterator iter =
                         olist.begin();
                     iter != olist.end(); ++iter)
                {
                    if ((! (*iter).isIndirect()) && (*iter).isName())
                    {
                        names.insert((*iter).getName());
                    }
                }

                std::map<std::string, QPDFObjectHandle> dict;
                int next_fake_key = 1;
                for (unsigned int i = 0; i < olist.size(); ++i)
                {
                    QPDFObjectHandle key_obj = olist.at(i);
                    QPDFObjectHandle val;
                    if (key_obj.isIndirect() || (! key_obj.isName()))
                    {
                        bool found_fake = false;
                        std::string candidate;
                        while (! found_fake)
                        {
                            candidate =
                                "/QPDFFake" +
                                QUtil::int_to_string(next_fake_key++);
                            found_fake = (names.count(candidate) == 0);
                            QTC::TC("qpdf", "QPDFObjectHandle found fake",
                                    (found_fake ? 0 : 1));
                        }
                        warn(context,
                             QPDFExc(
                                 qpdf_e_damaged_pdf,
                                 input->getName(), object_description, offset,
                                 "expected dictionary key but found"
                                 " non-name object; inserting key " +
                                 candidate));
                        val = key_obj;
                        key_obj = newName(candidate);
                    }
                    else if (i + 1 >= olist.size())
                    {
                        QTC::TC("qpdf", "QPDFObjectHandle no val for last key");
                        warn(context,
                             QPDFExc(
                                 qpdf_e_damaged_pdf,
                                 input->getName(), object_description, offset,
                                 "dictionary ended prematurely; "
                                 "using null as value for last key"));
                        val = newNull();
                    }
                    else
                    {
                        val = olist.at(++i);
                    }
                    dict[key_obj.getName()] = val;
                }
                object = newDictionary(dict);
            }
            olist_stack.pop_back();
            offset_stack.pop_back();
            if (state_stack.back() == st_top)
            {
                done = true;
            }
            else
            {
                olist_stack.back().push_back(object);
            }
        }
    }

    return object;
}

QPDFObjectHandle
QPDFObjectHandle::newIndirect(QPDF* qpdf, int objid, int generation)
{
    if (objid == 0)
    {
        // Special case: QPDF uses objid 0 as a sentinel for direct
        // objects, and the PDF specification doesn't allow for object
        // 0. Treat indirect references to object 0 as null so that we
        // never create an indirect object with objid 0.
        QTC::TC("qpdf", "QPDFObjectHandle indirect with 0 objid");
        return newNull();
    }

    return QPDFObjectHandle(qpdf, objid, generation);
}

QPDFObjectHandle
QPDFObjectHandle::newBool(bool value)
{
    return QPDFObjectHandle(new QPDF_Bool(value));
}

QPDFObjectHandle
QPDFObjectHandle::newNull()
{
    return QPDFObjectHandle(new QPDF_Null());
}

QPDFObjectHandle
QPDFObjectHandle::newInteger(long long value)
{
    return QPDFObjectHandle(new QPDF_Integer(value));
}

QPDFObjectHandle
QPDFObjectHandle::newReal(std::string const& value)
{
    return QPDFObjectHandle(new QPDF_Real(value));
}

QPDFObjectHandle
QPDFObjectHandle::newReal(double value, int decimal_places)
{
    return QPDFObjectHandle(new QPDF_Real(value, decimal_places));
}

QPDFObjectHandle
QPDFObjectHandle::newName(std::string const& name)
{
    return QPDFObjectHandle(new QPDF_Name(name));
}

QPDFObjectHandle
QPDFObjectHandle::newString(std::string const& str)
{
    return QPDFObjectHandle(new QPDF_String(str));
}

QPDFObjectHandle
QPDFObjectHandle::newOperator(std::string const& value)
{
    return QPDFObjectHandle(new QPDF_Operator(value));
}

QPDFObjectHandle
QPDFObjectHandle::newInlineImage(std::string const& value)
{
    return QPDFObjectHandle(new QPDF_InlineImage(value));
}

QPDFObjectHandle
QPDFObjectHandle::newArray()
{
    return newArray(std::vector<QPDFObjectHandle>());
}

QPDFObjectHandle
QPDFObjectHandle::newArray(std::vector<QPDFObjectHandle> const& items)
{
    return QPDFObjectHandle(new QPDF_Array(items));
}

QPDFObjectHandle
QPDFObjectHandle::newDictionary()
{
    return newDictionary(std::map<std::string, QPDFObjectHandle>());
}

QPDFObjectHandle
QPDFObjectHandle::newDictionary(
    std::map<std::string, QPDFObjectHandle> const& items)
{
    return QPDFObjectHandle(new QPDF_Dictionary(items));
}


QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, int objid, int generation,
			    QPDFObjectHandle stream_dict,
			    qpdf_offset_t offset, size_t length)
{
    return QPDFObjectHandle(new QPDF_Stream(
				qpdf, objid, generation,
				stream_dict, offset, length));
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf)
{
    QTC::TC("qpdf", "QPDFObjectHandle newStream");
    QPDFObjectHandle stream_dict = newDictionary();
    QPDFObjectHandle result = qpdf->makeIndirectObject(
	QPDFObjectHandle(
	    new QPDF_Stream(qpdf, 0, 0, stream_dict, 0, 0)));
    result.dereference();
    QPDF_Stream* stream = dynamic_cast<QPDF_Stream*>(result.obj.getPointer());
    stream->setObjGen(result.getObjectID(), result.getGeneration());
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, PointerHolder<Buffer> data)
{
    QTC::TC("qpdf", "QPDFObjectHandle newStream with data");
    QPDFObjectHandle result = newStream(qpdf);
    result.replaceStreamData(data, newNull(), newNull());
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::newStream(QPDF* qpdf, std::string const& data)
{
    QTC::TC("qpdf", "QPDFObjectHandle newStream with string");
    QPDFObjectHandle result = newStream(qpdf);
    result.replaceStreamData(data, newNull(), newNull());
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::newReserved(QPDF* qpdf)
{
    // Reserve a spot for this object by assigning it an object
    // number, but then return an unresolved handle to the object.
    QPDFObjectHandle reserved = qpdf->makeIndirectObject(
	QPDFObjectHandle(new QPDF_Reserved()));
    QPDFObjectHandle result =
        newIndirect(qpdf, reserved.objid, reserved.generation);
    result.reserved = true;
    return result;
}

QPDFObjectHandle
QPDFObjectHandle::shallowCopy()
{
    assertInitialized();

    if (isStream())
    {
	QTC::TC("qpdf", "QPDFObjectHandle ERR shallow copy stream");
	throw std::runtime_error(
	    "attempt to make a shallow copy of a stream");
    }

    QPDFObjectHandle new_obj;
    if (isArray())
    {
	QTC::TC("qpdf", "QPDFObjectHandle shallow copy array");
	new_obj = newArray(getArrayAsVector());
    }
    else if (isDictionary())
    {
	QTC::TC("qpdf", "QPDFObjectHandle shallow copy dictionary");
        new_obj = newDictionary(getDictAsMap());
    }
    else
    {
	QTC::TC("qpdf", "QPDFObjectHandle shallow copy scalar");
        new_obj = *this;
    }

    return new_obj;
}

void
QPDFObjectHandle::makeDirectInternal(std::set<int>& visited)
{
    assertInitialized();

    if (isStream())
    {
	QTC::TC("qpdf", "QPDFObjectHandle ERR clone stream");
	throw std::runtime_error(
	    "attempt to make a stream into a direct object");
    }

    int cur_objid = this->objid;
    if (cur_objid != 0)
    {
	if (visited.count(cur_objid))
	{
	    QTC::TC("qpdf", "QPDFObjectHandle makeDirect loop");
	    throw std::runtime_error(
		"loop detected while converting object from "
		"indirect to direct");
	}
	visited.insert(cur_objid);
    }

    if (isReserved())
    {
        throw std::logic_error(
            "QPDFObjectHandle: attempting to make a"
            " reserved object handle direct");
    }

    dereference();
    this->qpdf = 0;
    this->objid = 0;
    this->generation = 0;

    PointerHolder<QPDFObject> new_obj;

    if (isBool())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone bool");
	new_obj = new QPDF_Bool(getBoolValue());
    }
    else if (isNull())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone null");
	new_obj = new QPDF_Null();
    }
    else if (isInteger())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone integer");
	new_obj = new QPDF_Integer(getIntValue());
    }
    else if (isReal())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone real");
	new_obj = new QPDF_Real(getRealValue());
    }
    else if (isName())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone name");
	new_obj = new QPDF_Name(getName());
    }
    else if (isString())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone string");
	new_obj = new QPDF_String(getStringValue());
    }
    else if (isArray())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone array");
	std::vector<QPDFObjectHandle> items;
	int n = getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    items.push_back(getArrayItem(i));
	    items.back().makeDirectInternal(visited);
	}
	new_obj = new QPDF_Array(items);
    }
    else if (isDictionary())
    {
	QTC::TC("qpdf", "QPDFObjectHandle clone dictionary");
	std::set<std::string> keys = getKeys();
	std::map<std::string, QPDFObjectHandle> items;
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    items[*iter] = getKey(*iter);
	    items[*iter].makeDirectInternal(visited);
	}
	new_obj = new QPDF_Dictionary(items);
    }
    else
    {
	throw std::logic_error("QPDFObjectHandle::makeDirectInternal: "
			       "unknown object type");
    }

    this->obj = new_obj;

    if (cur_objid)
    {
	visited.erase(cur_objid);
    }
}

void
QPDFObjectHandle::makeDirect()
{
    std::set<int> visited;
    makeDirectInternal(visited);
}

void
QPDFObjectHandle::assertInitialized() const
{
    if (! this->initialized)
    {
	throw std::logic_error("operation attempted on uninitialized "
			       "QPDFObjectHandle");
    }
}

void
QPDFObjectHandle::assertType(char const* type_name, bool istype) const
{
    if (! istype)
    {
	throw std::logic_error(std::string("operation for ") + type_name +
			       " object attempted on object of wrong type");
    }
}

void
QPDFObjectHandle::assertNull()
{
    assertType("Null", isNull());
}

void
QPDFObjectHandle::assertBool()
{
    assertType("Boolean", isBool());
}

void
QPDFObjectHandle::assertInteger()
{
    assertType("Integer", isInteger());
}

void
QPDFObjectHandle::assertReal()
{
    assertType("Real", isReal());
}

void
QPDFObjectHandle::assertName()
{
    assertType("Name", isName());
}

void
QPDFObjectHandle::assertString()
{
    assertType("String", isString());
}

void
QPDFObjectHandle::assertOperator()
{
    assertType("Operator", isOperator());
}

void
QPDFObjectHandle::assertInlineImage()
{
    assertType("InlineImage", isInlineImage());
}

void
QPDFObjectHandle::assertArray()
{
    assertType("Array", isArray());
}

void
QPDFObjectHandle::assertDictionary()
{
    assertType("Dictionary", isDictionary());
}

void
QPDFObjectHandle::assertStream()
{
    assertType("Stream", isStream());
}

void
QPDFObjectHandle::assertReserved()
{
    assertType("Reserved", isReserved());
}

void
QPDFObjectHandle::assertIndirect()
{
    if (! isIndirect())
    {
	throw std::logic_error(
            "operation for indirect object attempted on direct object");
    }
}

void
QPDFObjectHandle::assertScalar()
{
    assertType("Scalar", isScalar());
}

void
QPDFObjectHandle::assertNumber()
{
    assertType("Number", isNumber());
}

bool
QPDFObjectHandle::isPageObject()
{
    // Some PDF files have /Type broken on pages.
    return (this->isDictionary() && this->hasKey("/Contents"));
}

bool
QPDFObjectHandle::isPagesObject()
{
    // Some PDF files have /Type broken on pages.
    return (this->isDictionary() && this->hasKey("/Kids"));
}

void
QPDFObjectHandle::assertPageObject()
{
    if (! isPageObject())
    {
	throw std::logic_error("page operation called on non-Page object");
    }
}

void
QPDFObjectHandle::dereference()
{
    if (this->obj.getPointer() == 0)
    {
        PointerHolder<QPDFObject> obj = QPDF::Resolver::resolve(
	    this->qpdf, this->objid, this->generation);
	if (obj.getPointer() == 0)
	{
	    QTC::TC("qpdf", "QPDFObjectHandle indirect to unknown");
	    this->obj = new QPDF_Null();
	}
        else if (dynamic_cast<QPDF_Reserved*>(obj.getPointer()))
        {
            // Do not resolve
        }
        else
        {
            this->reserved = false;
            this->obj = obj;
        }
    }
}

void
QPDFObjectHandle::warn(QPDF* qpdf, QPDFExc const& e)
{
    // If parsing on behalf of a QPDF object and want to give a
    // warning, we can warn through the object. If parsing for some
    // other reason, such as an explicit creation of an object from a
    // string, then just throw the exception.
    if (qpdf)
    {
        QPDF::Warner::warn(qpdf, e);
    }
    else
    {
        throw e;
    }
}
