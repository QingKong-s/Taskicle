<template>
  <div v-if="localTask" class="task-detail">
    <div class="detail-header">
      <div class="section id">#{{ localTask.task_id || localTask.id }}</div>
      <div class="detail-actions">
        <el-button v-if="!localTask.isNew" type="text" size="small" class="delete-btn"
          @click="onDeleteClick">删除</el-button>
        <el-button v-else type="primary" size="small" class="complete-btn" @click="onCompleteClick">完成</el-button>
      </div>
    </div>

    <div class="section name">
      <el-input v-model="localTask.task_name" type="input" class="field-name field"
        @blur="() => saveTask(['task_name'])" />
    </div>

    <div class="section row two-cols">
      <span class="col status-col">
        <label class="label">状态</label>

        <el-dropdown trigger="click" @command="onStateChange">
          <button type="button" class="capsule-btn state-btn" :style="{
            borderLeftColor: getStatus('state', localTask.status || localTask.state).color,
            background: getStatus('state', localTask.status || localTask.state).background,
            color: getStatus('state', localTask.status || localTask.state).color
          }">
            <el-icon>
              <CircleCheck />
            </el-icon>
            {{ getStatus('state', localTask.status || localTask.state).text }}
          </button>

          <template #dropdown>
            <el-dropdown-menu>
              <el-dropdown-item command="0">未开始</el-dropdown-item>
              <el-dropdown-item command="1">进行中</el-dropdown-item>
              <el-dropdown-item command="2">已完成</el-dropdown-item>
              <el-dropdown-item command="3">关闭</el-dropdown-item>
              <el-dropdown-item command="4">挂起</el-dropdown-item>
              <el-dropdown-item command="5">待验证</el-dropdown-item>
            </el-dropdown-menu>
          </template>
        </el-dropdown>

        <span style="width: 40px;"></span>

        <label class="label">优先级</label>

        <el-dropdown trigger="click" @command="onPriorityChange">
          <button type="button" class="capsule-btn priority-btn" :style="{
            borderLeftColor: getStatus('priority', localTask.priority).color,
            background: getStatus('priority', localTask.priority).background,
            color: getStatus('priority', localTask.priority).color
          }">
            <el-icon>
              <Flag />
            </el-icon>
            {{ getStatus('priority', localTask.priority).text }}
          </button>

          <template #dropdown>
            <el-dropdown-menu>
              <el-dropdown-item command="0">最低</el-dropdown-item>
              <el-dropdown-item command="1">较低</el-dropdown-item>
              <el-dropdown-item command="2">普通</el-dropdown-item>
              <el-dropdown-item command="3">较高</el-dropdown-item>
              <el-dropdown-item command="4">最高</el-dropdown-item>
            </el-dropdown-menu>
          </template>

        </el-dropdown>
      </span>
    </div>

    <div class="section">
      <label class="label">描述</label>
      <el-input v-model="localTask.description" type="textarea" :autosize="{ minRows: 4 }" class="field-desc field"
        @blur="() => saveTask(['description'])" />
    </div>

    <div class="section">
      <label class="label">属性</label>
      <el-descriptions :column="1" class="weak-desc">
        <el-descriptions-item label="创建时间">
          {{ new Date(localTask.create_at).toLocaleString() }}
        </el-descriptions-item>
        <el-descriptions-item label="更新时间">
          {{ new Date(localTask.update_at).toLocaleString() }}
        </el-descriptions-item>

        <el-descriptions-item label="过期时间">
          <el-date-picker v-model="localTask.expire_at" type="datetime" value-format="x" clearable
            @change="(val) => saveTask(['expire_at'])" />
        </el-descriptions-item>
      </el-descriptions>
    </div>

    <div class="section">
      <label class="label">
        关联任务
        <el-button type="text" size="small" @click="showSearch(1)">添加</el-button>
      </label>
      <ul class="list">
        <li v-for="rel in relatedTasks" :key="rel.relation_id" class="click-item">
          <el-icon>
            <Tickets />
          </el-icon>
          #{{ rel.relation_id }} {{ rel.name }}
          <el-button type="text" size="small" style="float:right" @click.stop="deleteRelation(rel)">删除</el-button>
        </li>
      </ul>
    </div>

    <div class="section">
      <label class="label">
        关联页面
        <el-button type="text" size="small" @click="showSearch(2)">添加</el-button>
      </label>

      <ul class="list">
        <li v-for="rel in documents" :key="rel.relation_id" class="click-item">
          <el-icon>
            <Document />
          </el-icon>
          #{{ rel.relation_id }} {{ rel.name }}
          <el-button type="text" size="small" style="float:right" @click.stop="deleteRelation(rel)">删除</el-button>
        </li>
      </ul>
    </div>

    <div class="section">
      <label class="label">
        文件
        <el-button type="text" size="small" @click="showSearch(4)">添加</el-button>
      </label>

      <ul class="list">
        <li v-for="rel in files" :key="rel.relation_id" class="click-item">
          <el-icon>
            <Paperclip />
          </el-icon>
          #{{ rel.relation_id }} {{ rel.name }}
          <el-button type="text" size="small" style="float:right" @click.stop="deleteRelation(rel)">删除</el-button>
        </li>
      </ul>
    </div>

    <div class="section">
      <el-tabs v-model="activeTab">
        <el-tab-pane label="全部" name="all" />
        <el-tab-pane label="评论" name="comment" />
        <el-tab-pane label="变更记录" name="changelog" />
      </el-tabs>

      <TaskChangeLog 
        ref="logCmp" 
        v-if="localTask" 
        :taskId="localTask.task_id || -1" 
        :mode="activeTab" 
      />
    </div>

  </div>
  <div v-else class="task-detail">
    <div class="section">请选择一个任务以查看或编辑</div>
  </div>
</template>

<script setup>
import { ref, watch, defineProps, defineEmits, computed, inject } from 'vue'
import { CircleCheck, Flag, Tickets, Document, Paperclip } from '@element-plus/icons-vue'
import { getStatus, showErrorMessage } from '../utils/utils'
import api from '../utils/api'
import TaskChangeLog from './TaskLogComment.vue'
import { ElMessage } from 'element-plus'

const props = defineProps({
  task: { type: Object, default: null }
})
const emit = defineEmits(['updated', 'deleted', 'created'])

const originalTask = ref(null)

const localTask = ref(null)
const logCmp = ref(null)

watch(() => props.task, (v) => {
  if (!v) {
    localTask.value = null
  } else {
    try {
      localTask.value = JSON.parse(JSON.stringify(v))
    } catch (e) {
      localTask.value = Object.assign({}, v)
    }

    if (localTask.value) {
      if (!localTask.value.task_name && localTask.value.name)
        localTask.value.task_name = localTask.value.name
      if (localTask.value.status == null && localTask.value.state != null)
        localTask.value.status = localTask.value.state
      if (localTask.value.state == null && localTask.value.status != null)
        localTask.value.state = localTask.value.status
    }

    try {
      originalTask.value = JSON.parse(JSON.stringify(localTask.value))
    } catch (e) {
      originalTask.value = Object.assign({}, localTask.value)
    }
  }
}, { immediate: true })

const activeTab = ref('all')

async function saveTask(changedFields = null) {
  if (!localTask.value) return
  if (localTask.value.isNew) {
    try {
      originalTask.value = JSON.parse(JSON.stringify(localTask.value))
    } catch (e) {
      originalTask.value = Object.assign({}, localTask.value)
    }
    return
  }
  const payload = { project_id: localTask.value.project_id }
  function hasChanges(fields) {
    if (!originalTask.value) return true
    if (!fields) {
      const keys = ['task_name', 'status', 'priority', 'description', 'expire_at']
      return keys.some(k => originalTask.value[k] !== localTask.value[k])
    }
    return fields.some(k => originalTask.value[k] !== localTask.value[k])
  }

  if (!hasChanges(changedFields)) return
  const id = localTask.value.task_id || localTask.value.id
  if (id) payload.task_id = id

  if (Array.isArray(changedFields)) {
    changedFields.forEach(k => { payload[k] = localTask.value[k] })
  } else {
    payload.task_name = localTask.value.task_name || localTask.value.name
    payload.status = localTask.value.status || localTask.value.state
    payload.priority = localTask.value.priority
    payload.description = localTask.value.description
    payload.expire_at = localTask.value.expire_at
  }

  try {
    const res = await api.post('/api/task_update', payload)
    const j = res.data
      if (j && j.r === 0) {
      ElMessage.success('保存成功')
      try {
        originalTask.value = JSON.parse(JSON.stringify(localTask.value))
      }
      catch (e) {
        originalTask.value = Object.assign({}, localTask.value)
      }
      emit('updated', JSON.parse(JSON.stringify(localTask.value)))
      if (logCmp.value && typeof logCmp.value.reload === 'function') {
        try { logCmp.value.reload() } catch (e) { /* ignore reload errors */ }
      }
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showErrorMessage(ElMessage, e)
  }
}

function onStateChange(cmd) {
  if (!localTask.value) return
  const v = Number(cmd)
  localTask.value.status = v
  localTask.value.state = v
  saveTask(['status'])
}
function onPriorityChange(cmd) {
  if (!localTask.value) return
  const v = Number(cmd)
  localTask.value.priority = v
  saveTask(['priority'])
}

function onDeleteClick() {
  if (!localTask.value) return
  try {
    emit('deleted', JSON.parse(JSON.stringify(localTask.value)))
  } catch (e) {
    emit('deleted', Object.assign({}, localTask.value))
  }
}

async function onCompleteClick() {
  if (!localTask.value) return
  const payload = {
    project_id: localTask.value.project_id,
    task_name: localTask.value.task_name,
    description: localTask.value.description,
    priority: localTask.value.priority,
    status: localTask.value.status,
    expire_at: localTask.value.expire_at,
  }

  try {
    const res = await api.post('/api/task_insert', payload)
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('任务已创建')
      const created = j.data || {}
      try {
        Object.assign(localTask.value, created)
      } catch (e) { /* ignore */ }
      delete localTask.value.isNew
      try {
        originalTask.value = JSON.parse(JSON.stringify(localTask.value))
      } catch (e) {
        originalTask.value = Object.assign({}, localTask.value)
      }

      try {
        emit('created', JSON.parse(JSON.stringify(localTask.value)))
      } catch (e) {
        emit('created', Object.assign({}, localTask.value))
      }
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showErrorMessage(ElMessage, e)
  }
}

const relations = ref([])

async function loadRelations() {
  if (!localTask.value) return
  const taskId = localTask.value.task_id
  const res = await api.get('/api/task_relation', { params: { task_id: taskId } })
  relations.value = res.data?.data || []
}

watch(() => props.task, () => {
  loadRelations()
})

const relatedTasks = computed(() =>
  relations.value.filter(r => r.relation_type === 1)
)
const documents = computed(() =>
  relations.value.filter(r => r.relation_type === 2)
)
const files = computed(() =>
  relations.value.filter(r => r.relation_type === 4)
)

const openSearch = inject('openSearch', null)

function showSearch(type) {
  if (!openSearch) {
    ElMessage.error('全局搜索不可用')
    return
  }
  openSearch({
    onSelect: async (item) => {
      if (!localTask.value) return
      const taskId = localTask.value.task_id
      const relationId = Number(item.entity_id)
      if (!relationId) {
        ElMessage.error('无效的关联 ID')
        return
      }
      try {
        const res = await api.post('/api/task_relation_insert',
          { task_id: taskId, relation_id: relationId, relation_type: Number(type) })
        if (res.data && res.data.r === 0) {
          ElMessage.success('添加成功')
          loadRelations()
        } else {
          showErrorMessage(ElMessage, res.data)
        }
      } catch (e) {
        showErrorMessage(ElMessage, e)
      }
    }
  })
}

async function deleteRelation(rel) {
  const taskId = localTask.value.task_id

  try {
    const res = await api.post('/api/task_relation_delete',
      { task_id: taskId, relation_id: rel.relation_id })
    if (res.data && res.data.r === 0) {
      ElMessage.success("删除成功")
      loadRelations()
    } else {
      showErrorMessage(ElMessage, res.data)
    }
  } catch (e) {
    showErrorMessage(ElMessage, e)
  }
}
</script>

<style scoped>
.task-detail {
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 18px;
}

.detail-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.detail-actions {
  display: flex;
  gap: 8px;
}

.delete-btn {
  color: #c00;
  padding: 2px 6px;
}

.feild {
  border: none;
  background: transparent;
  border-radius: 6px;
  transition: 0.15s;
}

.field-name {
  font-size: 18px;
}

.field-desc {
  font-size: 14px;
}


.capsule-btn {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 6px 14px;
  border-radius: 20px;
  border: 1px solid #dcdfe6;
  background: #f7f9fc;
  cursor: pointer;
  transition: 0.15s;
  font-size: 14px;
}

.capsule-btn:hover {
  background: #eef2f7;
}

.state-btn,
.priority-btn {
  border-left: 5px solid transparent;
}

.weak-desc .el-descriptions__body {
  background: #fafafa;
}

.list {
  display: flex;
  flex-direction: column;
  gap: 0;
  padding-left: 4px;
}

.click-item {
  display: flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  height: 44px;
  padding: 0 8px;
  border-radius: 0;
  transition: 0.15s;
}

.click-item:hover {
  background: #f5f7fa;
}

.section {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.row.two-cols {
  display: flex;
  gap: 18px;
}

.row.two-cols .col {
  display: flex;
  gap: 12px;
  align-items: center;
}

.label {
  font-size: 14px;
  color: #888;
}

.task-detail .field:focus,
.task-detail .field--no-border:focus,
.task-detail .field--with-border:focus,
.task-detail .desc-box:focus {
  outline: none;
  box-shadow: none;
}

.id {
  font-size: 20px;
  color: #666;
}
</style>