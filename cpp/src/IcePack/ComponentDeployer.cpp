// **********************************************************************
//
// Copyright (c) 2001
// Mutable Realms, Inc.
// Huntsville, AL, USA
//
// All Rights Reserved
//
// **********************************************************************

#include <Ice/Ice.h>

#include <IcePack/ComponentDeployer.h>
#include <IcePack/Admin.h>

#include <Yellow/Yellow.h>

#include <parsers/SAXParser.hpp>
#include <sax/HandlerBase.hpp>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stack>
#include <iterator>
#include <fstream>

using namespace std;
using namespace IcePack;

void IcePack::incRef(Task* p) { p->__incRef(); }
void IcePack::decRef(Task* p) { p->__decRef(); }

namespace IcePack
{

static string
toString(const XMLCh* ch)
{
    char* t = XMLString::transcode(ch);
    string s(t);
    delete[] t;
    return s;
}

//
// Create a directory. 
//
class CreateDirectory : public Task
{
public:

    CreateDirectory(const string& name)
	: _name(name)
    {
    }

    virtual void
    deploy()
    {
	if(mkdir(_name.c_str(), 0755) != 0)
	{
	    cerr << "Can't create directory: " << _name << endl;

	    Ice::SystemException ex(__FILE__, __LINE__);
	    ex.error = getSystemErrno();
	    throw ex;
	}
    }

    virtual void
    undeploy()
    {
	if(rmdir(_name.c_str()) != 0)
	{
	    cerr << "Can't remove directory: " << _name << endl;

	    Ice::SystemException ex(__FILE__, __LINE__);
	    ex.error = getSystemErrno();
	    throw ex;
	}
    }
    
private:

    string _name;
};

//
// Generate a configuration file from a property set.
//
class GenerateConfiguration : public Task
{
    class WriteConfigProperty : public unary_function<Ice::PropertyDict::value_type, string>
    {
    public:
	string 
	operator()(const Ice::PropertyDict::value_type& p) const
	{
		return p.first + "=" + p.second;
	}
    };

public:

    GenerateConfiguration(const string& file, const Ice::PropertiesPtr& properties) :
	_file(file),
	_properties(properties)
    {
    }

    virtual void
    deploy()  
    {
	ofstream configfile;
	configfile.open(_file.c_str(), ios::out);
	if(!configfile)
	{
	    cerr << "Can't create configuration file: " << _file << endl;
	    Ice::SystemException ex(__FILE__, __LINE__);
	    throw ex;
	}
	
	Ice::PropertyDict props = _properties->getPropertiesForPrefix("");
	transform(props.begin(), props.end(), ostream_iterator<string>(configfile,"\n"), WriteConfigProperty());
	configfile.close();
    }

    virtual void
    undeploy()
    {
	if(unlink(_file.c_str()) != 0)
	{
	    cerr << "Can't remove configuration file: " << _file << endl;

	    Ice::SystemException ex(__FILE__, __LINE__);
	    ex.error = getSystemErrno();
	    throw ex;	    
	}
    }

private:

    string _file;
    Ice::PropertiesPtr _properties;
};

//
// Register an offer with the yellow page service.
//
class RegisterOffer : public Task
{
public:

    RegisterOffer(const Yellow::AdminPrx& admin, const string& offer, const Ice::ObjectPrx& proxy) :
	_admin(admin),
	_offer(offer),
	_proxy(proxy)
    {
    }

    virtual void
    deploy()
    {
	_admin->add(_offer, _proxy);
    }

    virtual void
    undeploy()
    {
	try
	{
	    _admin->remove(_offer, _proxy);
	}
	catch(const Yellow::NoSuchOfferException&)
	{
	    //
	    // TODO: The offer doesn't exist anymore so no need to
	    // worry about removing it. We should print a warning
	    // though.
	    //
	}	
    }

private:

    Yellow::AdminPrx _admin;
    string _offer;
    Ice::ObjectPrx _proxy;
};

}

IcePack::ComponentErrorHandler::ComponentErrorHandler(ComponentDeployer& deployer) :
    _deployer(deployer)
{
}

void
IcePack::ComponentErrorHandler::warning(const SAXParseException& exception)
{
    string s = toString(exception.getMessage());
    cerr << "warning: " << s << endl;
}

void
IcePack::ComponentErrorHandler::error(const SAXParseException& exception)
{
    string s = toString(exception.getMessage());
    cerr << "error: " << s << endl;
}

void
IcePack::ComponentErrorHandler::fatalError(const SAXParseException& exception)
{
    string s = toString(exception.getMessage());
    cerr << "fatal:" << s << endl;
}

void
IcePack::ComponentErrorHandler::resetErrors()
{
}

IcePack::ComponentDeployHandler::ComponentDeployHandler(ComponentDeployer& deployer) :
    _deployer(deployer)
{
}

void
IcePack::ComponentDeployHandler::characters(const XMLCh *const chars, const unsigned int length)
{
    _elements.top().assign(toString(chars));
}

void
IcePack::ComponentDeployHandler::startElement(const XMLCh *const name, AttributeList &attrs)
{
    _elements.push("");

    string str = toString(name);

    if(str == "property")
    {
	_deployer.addProperty(getAttributeValue(attrs, "name"), getAttributeValue(attrs, "value"));
    }
}

void
IcePack::ComponentDeployHandler::endElement(const XMLCh *const name)
{
    _elements.pop();
}

string
IcePack::ComponentDeployHandler::getAttributeValue(const AttributeList& attrs, const string& name) const
{
    XMLCh* n = XMLString::transcode(name.c_str());
    const XMLCh* value = attrs.getValue(n);
    delete[] n;
    
    if(value == 0)
    {
	cerr << "Missing attribute '" << name << "'" << endl;
	return "";
    }

    return toString(value);
}

string
IcePack::ComponentDeployHandler::getAttributeValueWithDefault(const AttributeList& attrs, const string& name, 
							      const string& def) const
{
    XMLCh* n = XMLString::transcode(name.c_str());
    const XMLCh* value = attrs.getValue(n);
    delete[] n;
    
    if(value == 0)
    {
	return def;
    }
    else
    {
	return toString(value);
    }
}

string
IcePack::ComponentDeployHandler::toString(const XMLCh* ch) const
{
    return IcePack::toString(ch);
}

string
IcePack::ComponentDeployHandler::elementValue() const
{
    return _elements.top();
}

IcePack::ComponentDeployer::ComponentDeployer(const Ice::CommunicatorPtr& communicator) :
    _communicator(communicator),
    _properties(Ice::createProperties())
{
    _variables["datadir"] = _communicator->getProperties()->getProperty("IcePack.Data");
    assert(!_variables["datadir"].empty());

    try
    {
	_yellowAdmin = Yellow::AdminPrx::checkedCast(
	    _communicator->stringToProxy(_communicator->getProperties()->getProperty("IcePack.Yellow.Admin")));
    }
    catch(Ice::LocalException&)
    {
    }
}

void 
IcePack::ComponentDeployer::parse(const string& xmlFile, ComponentDeployHandler& handler)
{
    _error = 0;

    SAXParser* parser = new SAXParser;
    parser->setValidationScheme(SAXParser::Val_Never);

    try
    {
	ComponentErrorHandler err(*this);
	parser->setDocumentHandler(&handler);
	parser->setErrorHandler(&err);
	parser->parse(xmlFile.c_str());
    }
    catch(const XMLException& e)
    {
	cerr << "XMLException: " << toString(e.getMessage()) << endl;
    }
    int rc = parser->getErrorCount();
    delete parser;

    if(_error > 0 || rc > 0)
    {
	throw DeploymentException();
    }    
}

void
IcePack::ComponentDeployer::deploy()
{
    vector<TaskPtr>::iterator p;
    for(p = _tasks.begin(); p != _tasks.end(); p++)
    {
	try
	{
	    (*p)->deploy();
	}
	catch(const DeploymentException& ex)
	{
	    cerr << "Deploy: " << ex << endl;
	    undeployFrom(p);
	    throw;
	}
	catch(const Ice::SystemException& ex)
	{
	    cerr << "Deploy: " << ex << endl;
	    undeployFrom(p);
	    throw DeploymentException();;
	}
    }
}

void
IcePack::ComponentDeployer::undeploy()
{
    for(vector<TaskPtr>::reverse_iterator p = _tasks.rbegin(); p != _tasks.rend(); p++)
    {
	try
	{
	    (*p)->undeploy();
	}
	catch(const DeploymentException& ex)
	{
	    //
	    // TODO: we probably need to log the failure to execute
	    // this task so that the use can take necessary steps to
	    // ensure it's correctly undeployed.
	    //
	    cerr << "Undeploy: " << ex << endl;
	}
	catch(const Ice::SystemException& ex)
	{
	    cerr << "Undeploy: " << ex << endl;
	}
    }
}

void
IcePack::ComponentDeployer::createDirectory(const string& name)
{
    string path = _variables["datadir"] + substitute(name.empty() || name[0] == '/' ? name : "/" + name);
    _tasks.push_back(new CreateDirectory(path));
}

void
IcePack::ComponentDeployer::createConfigFile(const string& name)
{
    assert(!name.empty());
    _configFile = _variables["datadir"] + substitute(name[0] == '/' ? name : "/" + name);
    _tasks.push_back(new GenerateConfiguration(_configFile, _properties));
}

void
IcePack::ComponentDeployer::addProperty(const string& name, const string& value)
{
    _properties->setProperty(substitute(name), substitute(value));
}

void
IcePack::ComponentDeployer::addOffer(const string& offer, const string& proxy)
{
    if(!_yellowAdmin)
    {
	cerr << "Yellow admin not set, can't register the offer '" << offer << "'" << endl;
	_error++;
	return;	
    }

    Ice::ObjectPrx object = _communicator->stringToProxy(proxy);
    if(!object)
    {
	cerr << "Invalid proxy: " << proxy << endl;
	_error++;
	return;	
    }
    
    _tasks.push_back(new RegisterOffer(_yellowAdmin, offer, object));
}

//
// Substitute variables with their values.
//
string
IcePack::ComponentDeployer::substitute(const string& v) const
{
    string value(v);
    string::size_type beg;
    string::size_type end = 0;

    while((beg = value.find("${")) != string::npos)
    {
	end = value.find("}", beg);
	
	if(end == string::npos)
	{
	    cerr << "Malformed variable name in : " << value << endl;
	    break; // Throw instead?
	}

	
	string name = value.substr(beg + 2, end - beg - 2);
	map<string, string>::const_iterator p = _variables.find(name);
	if(p == _variables.end())
	{
	    cerr << "Unknown variable: " << name << endl;
	    break; // Throw instead?
	}

	value.replace(beg, end - beg + 1, p->second);
    }

    return value;
}

void
IcePack::ComponentDeployer::undeployFrom(vector<TaskPtr>::iterator p)
{
    if(p != _tasks.begin())
    {
	for(vector<TaskPtr>::reverse_iterator q(p); q != _tasks.rend(); q++)
	{
	    try
	    {
		(*q)->undeploy();
	    }
	    catch(DeploymentException& ex)
	    {
		cerr << "Undeploy: " << ex << endl;
	    }
	    catch(Ice::SystemException& ex)
	    {
		cerr << "Undeploy: " << ex << endl;
	    }
	}
    }
}

