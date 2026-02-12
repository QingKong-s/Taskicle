<template>
  <el-dialog v-model="visible" custom-class="search-dialog" :modal="true" :close-on-click-modal="true" width="520px">
    <template #title>
      搜索
    </template>
    <div style="display:flex; gap:8px; align-items:center;">
      <el-input v-model="keyword" placeholder="输入关键词后回车" @keyup.enter="doSearch" clearable @clear="onClear" />
      <el-button type="primary" @click="doSearch">搜索</el-button>
    </div>
    <div style="margin-top:12px; max-height:360px; overflow:auto;">
      <el-empty v-if="!loading && results.length === 0 && searched" description="无结果" />
      <el-skeleton v-else-if="loading" :rows="4" animated />
      <ul v-else style="list-style:none; padding:0; margin:0;">
        <li v-for="(it, idx) in results" :key="idx" style="padding:8px; cursor:pointer; border-bottom:1px solid #f0f0f0;" @click="select(it)">
          <div style="display:flex; justify-content:space-between; gap:8px; align-items:center;">
            <div>
              <div style="font-weight:600">{{ it.name }}</div>
              <div style="color: #888; font-size:12px">{{ typeText(it.type) }}  •  ID: {{ it.entity_id }}</div>
            </div>
            <div style="color:#999; font-size:12px" v-if="it.container_id >= 0">
                上级容器ID: {{ it.container_id }}
            </div>
          </div>
        </li>
      </ul>
    </div>
  </el-dialog>
</template>

<script setup>
import { ref, watch, nextTick, defineProps, defineEmits } from 'vue'
import api from '@/utils/api'
import { ElMessage } from 'element-plus'

const props = defineProps({
  modelValue: { type: Boolean, default: false }
})
const emit = defineEmits(['update:modelValue', 'select'])

const visible = ref(!!props.modelValue)
const keyword = ref('')
const results = ref([])
const loading = ref(false)
const searched = ref(false)

watch(() => props.modelValue, (v) => {
    visible.value = !!v;
    if (v) {
        nextTick(() => {
            const el = document.querySelector('.search-dialog input');
            if (el) el.focus()
        })
    }
})
watch(visible, (v) => emit('update:modelValue', v))

function typeText(t) {
  if (t === 1) return '页面组'
  if (t === 2) return '页面'
  if (t === 3) return '项目'
  if (t === 4) return '任务'
  if (t === 5) return '用户'
  return String(t)
}

function onClear() {
  results.value = []
  searched.value = false
}

async function doSearch() {
  const k = (keyword.value || '').trim()
  if (!k) {
    ElMessage.warning('请输入关键词')
    return
  }
  loading.value = true
  try {
    const res = await api.get('/api/search', { params: { keyword: k, count: 50, page: 0 } })
    const j = res.data
    if (j && j.r === 0) {
      const d = j.data || []
      results.value = Array.isArray(d) ? d : (d ? [d] : [])
      searched.value = true
    } else {
      results.value = []
      searched.value = true
      ElMessage.error(j.err_msg || '搜索失败')
    }
  } catch (e) {
    results.value = []
    searched.value = true
    ElMessage.error('请求失败')
  } finally {
    loading.value = false
  }
}

function select(item) {
  emit('select', item)
  visible.value = false
}
</script>

<style scoped>
.search-dialog .el-dialog__body { padding: 16px 24px; }
</style>
