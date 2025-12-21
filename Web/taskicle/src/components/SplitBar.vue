<template>
    <div class="splitter" @mousedown="onMouseDown"></div>
</template>

<script setup>

import { defineEmits } from 'vue';

const emit = defineEmits(['ondrag']);

function onMouseDown(e) {
    e.preventDefault();

    function onMouseMove(event) {
        emit('ondrag', event.movementX);
    }

    function onMouseUp() {
        document.removeEventListener('mousemove', onMouseMove);
        document.removeEventListener('mouseup', onMouseUp);
    }

    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
}
</script>

<style scoped>
.splitter {
    width: 10px;
    cursor: col-resize;
    height: 100%;
    user-select: none;
}
</style>