#!/usr/bin/python
#
# author:      Luciano Augusto Kruk
# website:     www.kruk.eng.br
#
# description: Descobrir e exportar os dados de saida de uma rodada do
#              modelica num formato mais simples para colocar em graficos ou
#              processar.

import sys;
import getopt
import scipy.io as io;
import numpy    as np;
import os;

#======================================#

def print_usage():
    print "Usage:\n"
    print "   momat2dat <[-m|--mat=] file.mat>\n\n"

#======================================#
# A matriz 'name' contem os nomes das variaveis por COLUNA!.
# Esta rotina retorna uma lista com os nomes das variaveis em 'name'.

def fn_nameT (name):
    nb_var = len(name[0]);
    nb_siz = len(name);

    #print "dim 'name' = %d x %d\n" % (nb_siz, nb_var)

    names = [];
    for idx_var in range(nb_var):
        var = [];
        for idx_siz in range(nb_siz):
            try:
                var.append(name[idx_siz][idx_var]);
            except IndexError:
                break

        # unir os elementos, mas separa-los por '':
        var_name = ''.join(var)

        # deblank:
        var_name = var_name.strip("\0 \t");

        # saida:
        print "var= %3d: len=%4d; %s" % (idx_var, len(var_name), var_name)
        names.append(var_name);

    return names

#======================================#
# Esta rotina vai procurar os dados das variaveis em 'names' dentros dos
# dados de saida do modelica.

def fn_get_data(res, names, lookfor):

    data    = [];
    for lf in lookfor:

        # verificar presenca da variavel:
        if (not lf in names):
            print "variable '%s' not available in data!" % lf

        else:
            idx     = names.index(lf);
            idxdata = res['dataInfo'][0][idx];
            rowdata = res['dataInfo'][1][idx] - 1;

            if (0):
                print res['dataInfo']
                print "idx     = %d" % idx
                print "idxdata = %d" % idxdata
                print "rowdata = %d" % rowdata

            print ":- found '%s' at #%3d; idxdata=%d; rowdata=%d" \
                    % (lf, idx, idxdata, rowdata)

            # extract data:
            aux = eval("res['data_%d'][%d]" % (idxdata, rowdata))

            # concatenate:
            if (len(data) == 0):
                data = aux;
            else:
                data = np.vstack((data, aux));

    return data.transpose()

#======================================#

def fn_open_mat(filename, args):
    if (not os.path.isfile(filename)):
        print "file '%s' not found!" % (filename)
        sys.exit(-1);

    res = io.loadmat(filename);

    if (0):
        for key,data in res.items():
            print key

    names = fn_nameT(res['name'])
    data  = fn_get_data(res, names, args)

    if (not len(names)):
        print "no variable defined!"
        sys.exit(-1);

    return data

#======================================#

def main(argv):
    try:
        opts, args = getopt.getopt(argv, "m:", ["mat="]);
    except getopt.GetoptError:
        print_usage()
        sys.exit(-1);

    names = [];
    for opt,arg in opts:

        if (opt in ("-m", "--mat")):
            data = fn_open_mat(arg, args)

#======================================#
if (__name__ == "__main__"):
    main(sys.argv[1:])
#======================================#
