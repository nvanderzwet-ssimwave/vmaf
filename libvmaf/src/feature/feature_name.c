/**
 *
 *  Copyright 2016-2020 Netflix, Inc.
 *
 *     Licensed under the BSD+Patent License (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         https://opensource.org/licenses/BSDplusPatent
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alias.h"
#include "dict.h"
#include "feature_name.h"
#include "opt.h"

typedef struct {
    const char *name, *alias;
} Alias;

static const Alias alias_list[] = {
    {
        .name = "motion_force_zero",
        .alias = "force",
    },
    {
        .name = "adm_enhn_gain_limit",
        .alias = "egl",
    },
    {
        .name = "vif_enhn_gain_limit",
        .alias = "egl",
    },
    {
        .name = "adm_norm_view_dist",
        .alias = "nvd",
    },
    {
        .name = "adm_ref_display_height",
        .alias = "rdh",
    },
};

static const char *key_alias(char *key)
{
    const unsigned n = sizeof(alias_list) / sizeof(alias_list[0]);
    for(unsigned i = 0; i < n; i++) {
        if (!strcmp(key, alias_list[i].name))
            return alias_list[i].alias;
    }
    return NULL;
}


char *vmaf_feature_name(char *name, char *key, double val,
                        char *buf, size_t buf_sz)
{
    if (!key) return name;
    if (!key_alias(key)) return name;

    memset(buf, 0, buf_sz);
    snprintf(buf, buf_sz - 1, "%s_%s_%g",
             vmaf_feature_name_alias(name), key_alias(key), val);
    return buf;
}

static size_t snprintfcat(char* buf, size_t buf_sz, char const* fmt, ...)
{
    va_list args;
    const size_t len = strnlen(buf, buf_sz);
    va_start(args, fmt);
    const size_t result = vsnprintf(buf + len, buf_sz - len, fmt, args);
    va_end(args);

    return result + len;
}

char *vmaf_feature_name_from_opts_dict(char *name, VmafOption *opts,
                                       VmafDictionary *opts_dict)
{
    const size_t buf_sz = VMAF_FEATURE_NAME_DEFAULT_BUFFER_SIZE;
    char buf[VMAF_FEATURE_NAME_DEFAULT_BUFFER_SIZE + 1] = { 0 };

    if (!opts || !opts_dict) {
        snprintfcat(buf, buf_sz, "%s", name);
    } else {
        snprintfcat(buf, buf_sz, "%s", vmaf_feature_name_alias(name));

        VmafOption *opt = NULL;
        for (unsigned i = 0; (opt = &opts[i]); i++) {
            if (!opt->name) break;
            for (unsigned j = 0; j < opts_dict->cnt; j++) {
                if (strcmp(opt->name, opts_dict->entry[j].key)) continue;
                if (!(opt->flags & VMAF_OPT_FLAG_FEATURE_PARAM)) continue;
                const char *key = opt->alias ? opt->alias : opt->name;
                const char *val = opts_dict->entry[j].val;
                snprintfcat(buf, buf_sz, "_%s_%s", key, val);
            }
        }
    }

    const size_t dst_sz = strnlen(buf, buf_sz) + 1;
    char *dst = malloc(dst_sz);
    if (!dst) return NULL;
    strncpy(dst, buf, dst_sz);
    return dst;
}

static int option_is_default(const VmafOption *opt, const void *data)
{
    if (!opt) return -EINVAL;
    if (!data) return -EINVAL;

    switch (opt->type) {
    case VMAF_OPT_TYPE_BOOL:
        return opt->default_val.b == *((bool*)data);
    case VMAF_OPT_TYPE_INT:
        return opt->default_val.i == *((int*)data);
    case VMAF_OPT_TYPE_DOUBLE:
        return opt->default_val.d == *((double*)data);
    default:
        return -EINVAL;
    }
}

char *vmaf_feature_name_from_options(char *name, VmafOption *opts, void *obj)
{
    if (!name) return NULL;
    if (!opts) return NULL;
    if (!obj) return NULL;

    VmafDictionary *opts_dict = NULL;
    char *output = NULL;

    VmafOption *opt = NULL;
    for (unsigned i = 0; (opt = &opts[i]); i++) {
        if (!opt->name) break;
        if (!(opt->flags & VMAF_OPT_FLAG_FEATURE_PARAM)) continue;

        const void *data = (uint8_t*)obj + opt->offset;
        if (option_is_default(opt, data)) continue;

        const size_t buf_sz = VMAF_FEATURE_NAME_DEFAULT_BUFFER_SIZE;
        const char buf[VMAF_FEATURE_NAME_DEFAULT_BUFFER_SIZE + 1] = { 0 };

        switch (opt->type) {
        case VMAF_OPT_TYPE_BOOL:
            snprintf(buf, buf_sz, "%d", *(bool*)data);
            break;
        case VMAF_OPT_TYPE_INT:
            snprintf(buf, buf_sz, "%d", *((int*)data));
            break;
        case VMAF_OPT_TYPE_DOUBLE:
            snprintf(buf, buf_sz, "%g", *((double*)data));
            break;
        default:
            break;
        }

        int err = vmaf_dictionary_set(&opts_dict, opt->name, buf, 0);
        if (err) goto exit;
    }

    output = vmaf_feature_name_from_opts_dict(name, opts, opts_dict);

exit:
    vmaf_dictionary_free(&opts_dict);
    return output;
}

VmafDictionary *
vmaf_feature_name_dict_from_provided_features(const char **provided_features,
                                              const VmafOption *opts, void *obj)
{
    VmafDictionary *dict = NULL;

    char *feature_name;
    for (unsigned i = 0; (feature_name = provided_features[i]); i++) {
        char *fn = vmaf_feature_name_from_options(feature_name, opts, obj);
        if (!fn) goto fail;
        int err = vmaf_dictionary_set(&dict, feature_name, fn, 0);
        free(fn);
        if (err) goto fail;
    }
    return dict;

fail:
    vmaf_dictionary_free(&dict);
    return -EINVAL;
}
