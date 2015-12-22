import os
import mxnet as mx
import random
import numpy as np
import argparse
import threading
import cv, cv2
import time

def list_image(root, recursive, exts):
    image_list = []
    if recursive:
        cat = {}
        for path, subdirs, files in os.walk(root):
            print(len(cat), path)
            for fname in files:
                fpath = os.path.join(path, fname)
                suffix = os.path.splitext(fname)[1].lower()
                if os.path.isfile(fpath) and (suffix in exts):
                    if path not in cat:
                        cat[path] = len(cat)
                    image_list.append((len(image_list), os.path.relpath(fpath, root), cat[path]))
    else:
        for fname in os.listdir(root):
            fpath = os.path.join(root, fname)
            suffix = os.path.splitext(fname)[1].lower()
            if os.path.isfile(fpath) and (suffix in exts):
                image_list.append((len(image_list), os.path.relpath(fpath, root), 0))
    return image_list

def write_list(path_out, image_list):
    with open(path_out, 'w') as fout:
        for i in xrange(len(image_list)):
            line = '%d\t'%image_list[i][0]
            for j in image_list[i][2:]:
                line += '%f\t'%j
            line += '%s\n'%image_list[i][1]
            fout.write(line)

def make_list(prefix_out, root, recursive, exts, num_chunks, train_ratio):
    image_list = list_image(root, recursive, exts)
    random.shuffle(image_list)
    N = len(image_list)
    chunk_size = (N+num_chunks-1)/num_chunks
    for i in xrange(num_chunks):
        chunk = image_list[i*chunk_size:(i+1)*chunk_size]
        if num_chunks > 1:
            str_chunk = '_%d'%i
        else:
            str_chunk = ''
        if train_ratio < 1:
            sep = int(chunk_size*train_ratio)
            write_list(prefix_out+str_chunk+'_train.lst', chunk[:sep])
            write_list(prefix_out+str_chunk+'_val.lst', chunk[sep:])
        else:
            write_list(prefix_out+str_chunk+'.lst', chunk)

def read_list(path_in):
    image_list = []
    with open(path_in) as fin:
        for line in fin.readlines():
            line = [i.strip() for i in line.strip().split('\t')]
            item = [int(line[0])] + [line[-1]] + [float(i) for i in line[1:-1]]
            image_list.append(item)
    return image_list

def write_record(args, image_list):
    source = image_list
    sink = []
    record = mx.recordio.MXRecordIO(args.prefix+'.rec', 'w')
    lock = threading.Lock()
    tic = [time.time()]
    def worker(i):
        item = source.pop(0)
        img = cv2.imread(os.path.join(args.root, item[1]))
        if args.center_crop:
            if img.shape[0] > img.shape[1]:
                margin = (img.shape[0] - img.shape[1])/2;
                img = img[margin:margin+img.shape[1], :]
            else:
                margin = (img.shape[1] - img.shape[0])/2;
                img = img[:, margin:margin+img.shape[0]]
        if args.resize:
            if img.shape[0] > img.shape[1]:
                newsize = (img.shape[0]*args.resize/img.shape[1], args.resize)
            else:
                newsize = (args.resize, img.shape[1]*args.resize/img.shape[0])
            img = cv2.resize(img, newsize)
        header = mx.recordio.IRHeader(0, item[2], item[0], 0)
        s = mx.recordio.pack_img(header, img, quality=args.quality)
        lock.acquire()
        record.write(s)
        sink.append(item)
        if len(sink)%1000 == 0:
            print(len(sink), time.time()-tic[0])
            tic[0] = time.time()
        lock.release()

    try:
        from multiprocessing.pool import ThreadPool
        multi_available = True
    except ImportError:
        print('multiprocessing not available, fall back to single threaded encoding')
        multi_available = False 

    if multi_available and args.num_thread > 1:
        p = ThreadPool(args.num_thread)
        p.map(worker, [i for i in range(len(source))])
        write_list(args.prefix+'.lst', sink)
    else:
        while len(source):
            worker(len(sink))

def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        description='Make an image record database by reading from\
        an image list or creating one')
    parser.add_argument('prefix', help='prefix of input/output files.')
    parser.add_argument('root', help='path to folder containing images.')

    cgroup = parser.add_argument_group('Options for creating image lists')
    cgroup.add_argument('--list', type=bool, default=False,
        help='If this is set im2rec will create image list(s) by traversing root folder\
        and output to <prefix>.lst.\
        Otherwise im2rec will read <prefix>.lst and create a database at <prefix>.rec')
    cgroup.add_argument('--exts', type=list, default=['.jpeg','.jpg'],
        help='list of acceptable image extensions.')
    cgroup.add_argument('--chunks', type=int, default=1, help='number of chunks.')
    cgroup.add_argument('--train_ratio', type=float, default=1.0,
        help='Ratio of images to use for training.')
    cgroup.add_argument('--recursive', type=bool, default=False,
        help='If true recursively walk through subdirs and assign an unique label\
        to images in each folder. Otherwise only include images in the root folder\
        and give them label 0.')

    rgroup = parser.add_argument_group('Options for creating database')
    rgroup.add_argument('--resize', type=int, default=0,
        help='resize the shorter edge of image to the newsize, original images will\
        be packed by default.')
    rgroup.add_argument('--center_crop', type=bool, default=False,
        help='specify whether to crop the center image to make it rectangular.')
    rgroup.add_argument('--quality', type=int, default=80,
        help='JPEG quality for encoding, 1-100.')
    rgroup.add_argument('--num_thread', type=int, default=1,
        help='number of thread to use for encoding. order of images will be different\
        from the input list if >1. the input list will be modified to match the\
        resulting order.')

    args = parser.parse_args()
    
    if args.list:
        make_list(args.prefix, args.root, args.recursive,
                  args.exts, args.chunks, args.train_ratio)
    else:
        image_list = read_list(args.prefix+'.lst')
        write_record(args, image_list)

if __name__ == '__main__':
    main()
