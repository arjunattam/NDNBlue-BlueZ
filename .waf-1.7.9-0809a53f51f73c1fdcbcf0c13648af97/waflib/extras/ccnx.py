#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! http://waf.googlecode.com/git/docs/wafbook/single.html#_obtaining_the_waf_file

'''

When using this tool, the wscript will look like:

	def options(opt):
	        opt.tool_options('ccnx')

	def configure(conf):
		conf.load('compiler_c ccnx')

	def build(bld):
		bld(source='main.cpp', target='app', use='CCNX')

Options are generated, in order to specify the location of ccnx includes/libraries.


'''
import sys
import re
from waflib import Utils,Logs,Errors
from waflib.Configure import conf
CCNX_DIR=['/usr','/usr/local','/opt/local','/sw']
CCNX_VERSION_FILE='ccn/ccn.h'
CCNX_VERSION_CODE='''
#include <ccn/ccn.h>
#include <stdio.h>
int main() { printf ("%d.%d.%d", ((CCN_API_VERSION/100000) % 100), ((CCN_API_VERSION/1000) % 100), (CCN_API_VERSION % 1000)); return 0; }
'''
def options(opt):
	opt.add_option('--ccnx',type='string',default='',dest='ccnx_dir',help='''path to where CCNx is installed, e.g. /usr/local''')
@conf
def __ccnx_get_version_file(self,dir):
	try:
		return self.root.find_dir(dir).find_node('%s/%s'%('include',CCNX_VERSION_FILE))
	except:
		return None
@conf
def ccnx_get_version(self,dir):
	val=self.check_cc(fragment=CCNX_VERSION_CODE,includes=['%s/%s'%(dir,'include')],execute=True,define_ret=True,mandatory=True)
	return val
@conf
def ccnx_get_root(self,*k,**kw):
	root=k and k[0]or kw.get('path',None)
	if root and self.__ccnx_get_version_file(root):
		return root
	for dir in CCNX_DIR:
		if self.__ccnx_get_version_file(dir):
			return dir
	if root:
		self.fatal('CCNx not found in %s'%root)
	else:
		self.fatal('CCNx not found, please provide a --ccnx argument (see help)')
@conf
def check_ccnx(self,*k,**kw):
	if not self.env['CC']:
		self.fatal('load a c compiler first, conf.load("compiler_c")')
	var=kw.get('uselib_store','CCNX')
	self.start_msg('Checking CCNx')
	root=self.ccnx_get_root(*k,**kw);
	self.env.CCNX_VERSION=self.ccnx_get_version(root)
	self.env['INCLUDES_%s'%var]='%s/%s'%(root,"include");
	self.env['LIB_%s'%var]="ccn"
	self.env['LIBPATH_%s'%var]='%s/%s'%(root,"lib")
	self.end_msg(self.env.CCNX_VERSION)
	if Logs.verbose:
		Logs.pprint('CYAN','	ccnx include : %s'%self.env['INCLUDES_%s'%var])
		Logs.pprint('CYAN','	ccnx lib     : %s'%self.env['LIB_%s'%var])
		Logs.pprint('CYAN','	ccnx libpath : %s'%self.env['LIBPATH_%s'%var])
