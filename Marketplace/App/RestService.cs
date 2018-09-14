using System;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

using Xamarin.Forms;

using Newtonsoft.Json;
using PCLAppConfig;
using ModernHttpClient;

using Marktplaats.Models;

namespace Marktplaats.Data
{
    public class RestService
    {
        public static readonly NameValueCollection<PCLAppConfig.Infrastructure.Setting> AppSettings = ConfigurationManager.AppSettings;
        public static string AuthHeader;
        public static User AuthUser;

        public class ConflictException : Exception { };

        private HttpClient client;

        public RestService()
        {
            client = new HttpClient(new NativeMessageHandler());
            client.MaxResponseContentBufferSize = 256000;
            client.DefaultRequestHeaders.TryAddWithoutValidation("Authorization", AuthHeader);
            FFImageLoading.ImageService.Instance.Config.HttpClient = client;
        }

        public async Task<T> Get<T>(string uriString, params object[] args) where T : class
        {
            var uri = GetCombinedUri(uriString, args);
            var response = await client.GetAsync(uri);

            try { return await GetObjectFromResponse<T>(response); }
            catch { throw; }
        }

        public async Task<T> Put<T>(string uriString, object content, params object[] args) where T : class
        {
            var uri = GetCombinedUri(uriString, args);

            var contentJson = JsonConvert.SerializeObject(content);
            var contentJsonFormatted = new StringContent(contentJson, Encoding.UTF8, "application/json");
            var response = await client.PutAsync(uri, contentJsonFormatted);

            try { return await GetObjectFromResponse<T>(response); }
            catch { throw; }
        }

        public async Task<T> Post<T>(string uriString, object content, params object[] args) where T : class
        {
            var uri = GetCombinedUri(uriString, args);

            var contentJson = JsonConvert.SerializeObject(content);
            var contentJsonFormatted = new StringContent(contentJson, Encoding.UTF8, "application/json");
            var response = await client.PostAsync(uri, contentJsonFormatted);

            try { return await GetObjectFromResponse<T>(response); }
            catch { throw; }
        }

        public async Task<T> Delete<T>(string uriString, params object[] args) where T : class
        {
            var uri = GetCombinedUri(uriString, args);
            var response = await client.DeleteAsync(uri);

            try { return await GetObjectFromResponse<T>(response); }
            catch { throw; }
        }

        private async Task<T> GetObjectFromResponse<T>(HttpResponseMessage response) where T : class
        {
            switch (response.StatusCode)
            {
                case HttpStatusCode.Unauthorized:
                    throw new UnauthorizedAccessException();
                case HttpStatusCode.Conflict:
                    throw new ConflictException();
                case HttpStatusCode.Created:
                case HttpStatusCode.OK:
                    return await DeserializeObject<T>(response);
            }
            return null;
        }

        private async Task<T> DeserializeObject<T>(HttpResponseMessage response) where T : class
        {
            var jsonString = await response.Content.ReadAsStringAsync();
            return JsonConvert.DeserializeObject<T>(jsonString);
        }

        private Uri GetCombinedUri(string uriString, object[] args)
        {
            // Return a Uri object with the params added to the uriString
            return new Uri(string.Format(uriString, args));
        }
        
        public UriImageSource BuildImageSource(int imageId)
        {
            if(imageId < 0)
            {
                throw new ArgumentOutOfRangeException();
            }

            return BuildImageSource(string.Format(AppSettings["ApiUri.ImagesFile"], imageId));
        }

        public UriImageSource BuildImageSource(string path)
        {
            var imageUri = new Uri(path);
            return new UriImageSource
            {
                Uri = imageUri,
                // Use image caching for faster image loading and reuse
                CachingEnabled = true,
                CacheValidity = new TimeSpan(1, 0, 0, 0),
            };
        }
    }
}
