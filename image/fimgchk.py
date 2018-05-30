#!usr/bin/python

import skimage.io as imgio
import os
from shutil import copyfile
import sys


debug_output = True

def __output(message):
	if debug_output:
		print(message)

def move_to(filename, dst_path = '', fl_copy = False, fl_correct = False):
	if dst_path == '':
		return [filename,fl_correct]
	else:
		__output('moving '+filename+' to '+dst_path)
		fn = os.path.basename(filename)
		try:
			if (fl_copy):
				copyfile(filename,os.path.join(dst_path,fn))
			else:
				os.rename(filename,os.path.join(dst_path,fn))
		except FileExistsError:
			__output("Warning: "+os.path.join(dst_path,fn)+" already exist. copy/move skiped")
			return [filename,fl_correct]
		else:
			return [filename,fl_correct]
	
def chk_file(filename, corrects_path = '', incorrects_path = '', fl_copy = False):
	__output('chk_file '+filename)
	try:
		img = imgio.imread(filename)
	except:
		return move_to(filename,incorrects_path, fl_copy, False)
	else:
		if chk_hists(img):
			return move_to(filename,corrects_path, fl_copy, True)
		else:
			return move_to(filename,incorrects_path, fl_copy, False)
	__output('------------------------------------------------')

	
	
def chk_dir(dirname, corrects_path = '', incorrects_path = '', fl_copy = False):
	__output('chk_dir '+dirname)
	for filename in os.listdir(dirname):
		full_filename = os.path.join(dirname,filename)
		chk_file(full_filename, corrects_path, incorrects_path, fl_copy)
	

	
	
def chk(target_names, corrects_path = '', incorrects_path = '', fl_copy = False):
	__output([target_names,fl_copy,corrects_path,incorrects_path])
	if corrects_path != '' and not os.path.exists(corrects_path):
		os.makedirs(corrects_path)
	if incorrects_path != '' and not os.path.exists(incorrects_path):
		os.makedirs(incorrects_path)
	result = []
	for nm in target_names:
		if os.path.isdir(nm):
			result.append(chk_dir(nm, corrects_path, incorrects_path, fl_copy))
		else:
			#there may be something other than files and folders. but not today.
			result.append(chk_file(nm, corrects_path, incorrects_path, fl_copy))
	return result
	
	
if __name__ == '__main__':
	import argparse
	parser = argparse.ArgumentParser(description = 'checking the correctness (opening) of images or all images in folder')
	parser.add_argument('target_names', nargs='+', action='store', help='name of file(-s) or directory(-s) to check processing')
	parser.add_argument('-shuffle', nargs = 2, metavar=('PATH_CORRECT','PATH_INCORRECT'), action='store', default=['',''], \
									help = 'if you want to divide and MOVE (not copy) the images into folders PATH_CORRECT and PATH_INCORRECT')
	parser.add_argument('-copy', action='store_true', default=False, \
									help = 'if you want to shuffle with COPY files into folders specified with -shuffle option')
	args = parser.parse_args()
	result = chk(args.target_names, args.shuffle[0], args.shuffle[1], args.copy)
		
	
	
	
		