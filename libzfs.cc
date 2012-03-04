#include <v8.h>
#include <node.h>
#include <libzfs.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <list>

typedef struct zfs_cfg {
	nvlist_t *pool_cfg ;
	zpool_handle_t *zph ;
	char *name ;
} zfs_cfg_t ;

using namespace v8;

class ZfsInterface : node::ObjectWrap {

	public:
		static void Initialize(Handle<Object> target);

	protected:
		ZfsInterface() ;
		~ZfsInterface() ;

		static Handle<Value> New(const Arguments& args) ;
		static Handle<Value> Version(const Arguments& args) ;
		static Handle<Value> ListPools(const Arguments& args) ;

	private:
		libzfs_handle_t *zfs_handle ;
		static Persistent<FunctionTemplate> pftpl ;

} ;

Persistent<FunctionTemplate> ZfsInterface::pftpl ;

ZfsInterface::ZfsInterface() : node::ObjectWrap()
{
	if((zfs_handle = libzfs_init()) == NULL)
		throw(strerror(errno)) ;

}

ZfsInterface::~ZfsInterface()
{
	if(zfs_handle != NULL) {
		libzfs_fini(zfs_handle) ;
	}
}

void ZfsInterface::Initialize(Handle<Object> target)
{
	HandleScope scope;
	Local<FunctionTemplate> ftpl = FunctionTemplate::New(ZfsInterface::New);
	pftpl = Persistent<FunctionTemplate>::New(ftpl);
	pftpl->InstanceTemplate()->SetInternalFieldCount(1);
	pftpl->SetClassName(String::NewSymbol("zfshandle"));
	NODE_SET_PROTOTYPE_METHOD(pftpl , "version" , ZfsInterface::Version);
	NODE_SET_PROTOTYPE_METHOD(pftpl , "list" , ZfsInterface::ListPools);
	target->Set(String::NewSymbol("zfshandle"), pftpl->GetFunction());
}

Handle<Value> ZfsInterface::New(const Arguments& args)
{
	HandleScope scope;
	ZfsInterface *zfs ;

	try {
		zfs = new ZfsInterface();
	} catch (char const *msg) {
		return (ThrowException(Exception::Error(String::New(msg))));
	}

	zfs->Wrap(args.Holder());

	return (args.This());
}

Handle<Value> ZfsInterface::Version(const Arguments &args)
{
	double version = 0.1 ;
	return(Number::New(version)) ;
}

int poolIterate(zpool_handle_t *zph, void *data)
{
	std::list<zfs_cfg_t> *plist = (std::list<zfs_cfg_t>*)data ;
	zfs_cfg_t pcfg ;
	pcfg.zph = zph ;
	if(nvlist_lookup_nvlist(zpool_get_config(zph, NULL) , ZPOOL_CONFIG_VDEV_TREE, &pcfg.pool_cfg) == 0) { 
		pcfg.name = strdup(zpool_get_name(zph)) ;
		plist->push_back(pcfg) ;
		return(0) ;
	} else {
		return(1) ;
	}

	return(0) ;
}

Handle<Value> ZfsInterface::ListPools(const Arguments &args)
{
	HandleScope scope;
	int j = 0 ;
	uint64_t psize ;
	char prop_buf[MAXPATHLEN] ;
	std::list<zfs_cfg_t> *plist = new std::list<zfs_cfg_t>() ;
	Local<Object> pools = Object::New() ;
	Local<Array> poolNames = Array::New() ;
	ZfsInterface *obj = ObjectWrap::Unwrap<ZfsInterface>(args.This());

	if(zpool_iter(obj->zfs_handle , poolIterate , plist) != 0)
		return(Object::New()) ;

	std::list<zfs_cfg_t>::iterator i ;
	for(i = plist->begin() ; i != plist->end() ; ++i , j++) {
		Local<Object> o = Object::New() ;
		o->Set(String::NewSymbol("name"), String::New(i->name));
		o->Set(String::NewSymbol("version"), Number::New(zpool_get_prop_int(i->zph , ZPOOL_PROP_VERSION , NULL)));
		o->Set(String::NewSymbol("size"), Number::New(zpool_get_prop_int(i->zph , ZPOOL_PROP_SIZE , NULL)));
		o->Set(String::NewSymbol("dedup"), Number::New(zpool_get_prop_int(i->zph , ZPOOL_PROP_DEDUPRATIO , NULL)));
		o->Set(String::NewSymbol("ddedup"), Number::New(zpool_get_prop_int(i->zph , ZPOOL_PROP_DEDUPDITTO , NULL)));
		o->Set(String::NewSymbol("alloc"), Number::New(zpool_get_prop_int(i->zph , ZPOOL_PROP_ALLOCATED , NULL)));
		o->Set(String::NewSymbol("free"), Number::New(zpool_get_prop_int(i->zph , ZPOOL_PROP_FREE, NULL)));
		o->Set(String::NewSymbol("cap"), Number::New(zpool_get_prop_int(i->zph , ZPOOL_PROP_CAPACITY, NULL)));
		if(zpool_get_prop(i->zph , ZPOOL_PROP_HEALTH , prop_buf , ZFS_MAXPROPLEN , NULL) == 0) {
			o->Set(String::NewSymbol("health"), String::New(prop_buf));
		}
		poolNames->Set(j , o) ;
		free(i->name) ;
	}

	pools->Set(String::NewSymbol("zpool-list"), poolNames);
	delete plist ;
	return scope.Close(pools) ;
}

extern "C" void
init (Handle<Object> target)
{
	ZfsInterface::Initialize(target);
}

