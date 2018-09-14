using System;
using System.Threading.Tasks;

using Xamarin.Forms;
using Android.App;

using Microsoft.IdentityModel.Clients.ActiveDirectory;

using Marktplaats.Data.Authentication;

[assembly: Dependency(typeof(Marktplaats.Droid.Helper.Authenticator))]
namespace Marktplaats.Droid.Helper
{
    class Authenticator : IAuthenticator
    {
        public async Task<AuthenticationResult> Authenticate(string authority, string resource, string clientId, string returnUri)
        {
            try
            {
                var authContext = new AuthenticationContext(authority, false);

#if DEBUG
                authContext.TokenCache.Clear();
#endif

                var uri = new Uri(returnUri);
                var platformParams = new PlatformParameters((Activity)Forms.Context);
                var authResult = await authContext.AcquireTokenAsync(resource, clientId, uri, platformParams);
                return authResult;
            }
            catch
            {
                return null;
            }
        }
    }
}