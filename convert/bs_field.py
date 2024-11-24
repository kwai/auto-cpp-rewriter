import re
import sys
import logging
import fire
import sh
import json
import codecs

LOG_FORMAT = "%(asctime)s - %(levelname)s [%(filename)s:%(lineno)s - %(funcName)s] - %(message)s"
logging.basicConfig(level=logging.INFO, format=LOG_FORMAT)
logger = logging.getLogger(__name__)

def find_bs_field(filename: str='data/adlog_fields.json'):
    config = json.load(codecs.open(filename, 'r', 'utf-8'))
    all_bs_fields = set()
    for name in config:
        feature = config[name]
        bs_fields = set()
        logger.info('name: %s', name)
        for x in feature['adlog_fields']:
            for int_var_name in feature['int_var']:
                # p1 = re.compile('(\(\*(.*)find\(%s\)->second.list\(\).begin\(\)\))(.*)' % (int_var_name))
                # if p1.search(x) != None:
                #     s = re.sub(p1, '\\2.key:%d.\\3' % (feature['int_var'][int_var_name]), x).replace('()', '')
                #     bs_fields.add(s)
                #     logger.info('s: %s', s)
                #     break

                p = re.compile('(find\(%s\))->second' % (int_var_name))
                if p.search(x) != None:
                    s = re.sub(p, 'key:%d' % (feature['int_var'][int_var_name]), x).replace('()', '')
                    bs_fields.add(s)
                    logger.info('s: %s', s)
                    break

            for common_info_var_name in feature['common_info_var']:
                p = re.compile('(\(\*(.*)\.begin\(\)\)).(map[a-z0-9_]+)')
                if p.search(x) != None:
                    logger.info('p.findall: %s', p.findall(x))
                    s = re.sub(p, '\\2.key:%d' % (feature['common_info_var'][common_info_var_name]), x).replace('()', '')
                    bs_fields.add(s + '.key')
                    bs_fields.add(s + '.value')
                    logger.info('common_info s: %s.key', s)
                    logger.info('common_info s: %s.value', s)
                    break

                p1 = re.compile('(\(\*(.*)\.begin\(\)\)).((int|float|string)[a-z0-9_]+)')
                if p1.search(x) != None:
                    logger.info('p1.findall: %s', p1.findall(x))
                    s = re.sub(p1, '\\2.key:%d' % (feature['common_info_var'][common_info_var_name]), x).replace('()', '')
                    bs_fields.add(s)
                    logger.info('common_info s: %s', s)
                    break

                p2 = re.compile('((.*)\.common_info_attr\(i\)).(map[a-z0-9_]+)')
                if p2.search(x) != None:
                    logger.info('p.findall: %s', p2.findall(x))
                    s = re.sub(p2, '\\2.common_info_attr.key:%d' % (feature['common_info_var'][common_info_var_name]), x).replace('()', '')
                    bs_fields.add(s + '.key')
                    bs_fields.add(s + '.value')
                    logger.info('common_info s: %s.key', s)
                    logger.info('common_info s: %s.value', s)
                    break

                p3 = re.compile('((.*)\.common_info_attr\(i\)).((int|float|string)[a-z0-9_]+)')
                if p3.search(x) != None:
                    logger.info('p3.findall: %s', p3.findall(x))
                    s = re.sub(p3, '\\2.common_info_attr.key:%d' % (feature['common_info_var'][common_info_var_name]), x).replace('()', '')
                    bs_fields.add(s)
                    logger.info('common_info s: %s', s)
                    break

            bs_fields.add(x.replace('()', ''))

        arr = []
        for x in bs_fields:
            if x.startswith('(*'):
                continue
            if x.endswith('name_value'):
                continue
            if x.endswith('adlog.item(pos)'):
                continue
            if x.endswith('.empty'):
                continue
            if x.find('common_info_attr(i).') > 0:
                continue
            if x.find('has_') > 0:
                continue

            s = x.replace('item(pos)', 'item')
            p4 = re.compile('(.*).list_value(\(\d+\))')
            if p4.search(x) != None:
                logger.info('p4.findall: %s', p4.findall(x))
                s = re.sub(p4, '\\1.list_value', s)

            if s.find('(') > 0:
                continue

            p = re.compile('^[a-z0-9A-Z_:\.]+$')
            if not p.match(s):
                continue

            arr.append(s)
            all_bs_fields.add(s)

        feature['bs_fields'] = arr

    json.dump(config, codecs.open('data/adlog_fields_bs.json', 'w', 'utf-8'), ensure_ascii=False, indent=4)

    all_bs_fields_arr = list(all_bs_fields)
    json.dump(all_bs_fields_arr, codecs.open('data/all_bs_fields.json', 'w', 'utf-8'), ensure_ascii=False, indent=4)

    bs_fields_enum = [x.replace('.', '_').replace(':', '_') for x in all_bs_fields_arr]
    logger.info('len: %d, %d', len(all_bs_fields_arr), len(bs_fields_enum))

    feature_config_map = json.load(codecs.open('data/feature_config_map.json', 'r', 'utf-8'))['mapping']

    enum_arr = []
    content = "enum BsFieldEnum {\n"
    for i in range(len(all_bs_fields_arr)):
        if all_bs_fields_arr[i] in feature_config_map:
            enum_value = int(feature_config_map[all_bs_fields_arr[i]][1])
            enum_name = all_bs_fields_arr[i].replace('.', '_').replace(':', '_')
            enum_arr.append((enum_name, enum_value))
            # content += '    %s = %d,\n' % (enum_name, enum_value)
        else:
            logger.info("cannot find id for %s", all_bs_fields_arr[i])

    enum_arr.sort(key=lambda x: x[1])

    for (enum_name, enum_value) in enum_arr:
        content += '    %s = %d,\n' % (enum_name, enum_value)

    content += '}'

    codecs.open('data/bs_fields_enum.h', 'w', 'utf-8').write(content)

if __name__ == '__main__':
    fire.Fire()
