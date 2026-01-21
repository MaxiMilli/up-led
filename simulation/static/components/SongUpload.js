export const SongUpload = {
   template: `
    <div class="w-full p-4 mb-8">
      <div class="flex flex-col gap-4 items-start">
        <div class="flex-1">
          <label class="block text-sm font-medium text-gray-700 mb-1">TSN File</label>
          <input
            type="file"
            id="tsnFile"
            accept=".tsn"
            @change="handleFileChange"
            class="block w-full text-sm text-gray-500
                   file:mr-4 file:py-2 file:px-4
                   file:rounded-full file:border-0
                   file:text-sm file:font-semibold
                   file:bg-blue-50 file:text-blue-700
                   hover:file:bg-blue-100"
          >
        </div>
        <div class="flex-1">
          <label class="block text-sm font-medium text-gray-700 mb-1">Song Name</label>
          <input
            type="text"
            v-model="songName"
            class="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
          >
        </div>
        <button
          @click="uploadSong"
          class="bg-blue-500 text-white px-4 py-2 rounded hover:bg-blue-600"
        >
          Upload Song
        </button>
      </div>
    </div>
  `,
   data () {
      return {
         selectedFile: null,
         songName: ''
      }
   },
   methods: {
      handleFileChange (event) {
         this.selectedFile = event.target.files[0]
      },
      async uploadSong () {
         if (!this.selectedFile) {
            alert('Please select a file')
            return
         }
         if (!this.songName.trim()) {
            alert('Please enter a song name')
            return
         }

         try {
            const fileContent = await this.selectedFile.text()
            const myHeaders = new Headers()
            myHeaders.append("Content-Type", "text/plain")

            const requestOptions = {
               method: "POST",
               headers: myHeaders,
               body: fileContent,
               redirect: "follow"
            }

            const response = await fetch(`http://Samuels-MacBook-Pro.local:8000/songs/import/${this.songName}`, requestOptions)
            const result = await response.text()
            console.log(result)

            // Reset form
            this.selectedFile = null
            this.songName = ''
            document.getElementById('tsnFile').value = ''

            // Emit event to parent to refresh song list
            this.$emit('song-uploaded')

         } catch (error) {
            console.error(error)
            alert('Upload failed')
         }
      }
   }
} 
