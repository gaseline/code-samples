using System.Collections.Generic;
using System.Linq;
using System.Net;

using Microsoft.EntityFrameworkCore;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Authorization;

using MoreLinq;
using AutoMapper;

using MarktplaatsApi.Models;
using MarktplaatsApi.Services;
using MarktplaatsApi.ViewModels;
using MarktplaatsApi.Authentication;

namespace MarktplaatsApi.Controllers
{
    [Authorize]
    [Route("api/[controller]")]
    public class ProductsController : Controller
    {
        private ProductRepository productRepository;

        public ProductsController(IMapper mapper)
        {
            productRepository = new ProductRepository(mapper);
        }

        // GET: api/Products
        [HttpGet]
        public IEnumerable<Product> GetProducts(int page = 0, int pageSize = 10, 
            string filterString = "", int filterCategoryId = -1, bool personalOverview = false)
        {
            return productRepository.GetProductsByPage(page, pageSize, 
                filterString, filterCategoryId, personalOverview, User.GetId());
        }

        // GET: api/Products/5
        [HttpGet("{id}", Name = "GetProduct")]
        [ProducesResponseType(typeof(Product), (int)HttpStatusCode.OK)]
        public IActionResult GetProduct(int id)
        {
            var product = productRepository.FindById(id, x => x.User);
            return GetHttpResultForNullCheck(product);
        }

        // GET: api/Products/5/Category
        [HttpGet("{id}/category")]
        public IActionResult GetProductCategory(int id)
        {
            var product = productRepository.FindById(id, e => e.Category);
            return GetHttpResultForNullCheck(product.Category);
        }

        // GET: api/Products/5/Images
        [HttpGet("{id}/images")]
        public IActionResult GetProductImages(int id)
        {
            var images = productRepository.GetImagesByProduct(id);
            return GetHttpResultForNullCheck(images);
        }

        // GET: api/Products/5/Thumbnail
        [HttpGet("{id}/thumbnail")]
        public IActionResult GetProductThumbnail(int id)
        {
            var images = productRepository.GetImagesByProduct(id);

            if(images == null || images.Count() == 0)
            {
                return NotFound();
            }

            // The image containing the lowest OrderPosition will be used as thumbnail
            var thumbnail = images.MinBy(e => e.OrderPosition);

            return Ok(thumbnail);
        }

        // GET: api/Products/5/Bids
        [HttpGet("{id}/bids")]
        public IActionResult GetProductBids(int id)
        {
            var bids = productRepository.GetBidsByProduct(id);
            return GetHttpResultForNullCheck(bids);
        }

        // GET: api/Products/5/TopBid
        [HttpGet("{id}/topbid")]
        public IActionResult GetProductTopBid(int id)
        {
            var topBid = productRepository.GetTopBidByProduct(id);
            return GetHttpResultForNullCheck(topBid);
        }

        // PUT: api/Products/5
        [HttpPut("{id}")]
        [ProducesResponseType(typeof(Product), (int)HttpStatusCode.OK)]
        public IActionResult PutProduct(int id, [FromBody] ProductViewModel productViewModel)
        {
            if (!ModelState.IsValid)
            {
                return BadRequest(ModelState);
            }

            var editedProduct = productRepository.Edit(id, productViewModel, User.GetId());

            if(editedProduct == null)
            {
                return BadRequest();
            }

            try
            {
                productRepository.SaveChanges();
            }
            catch (DbUpdateConcurrencyException)
            {
                if (!ProductExists(id))
                {
                    return NotFound();
                }
                else
                {
                    throw;
                }
            }

            return Ok(editedProduct);
        }

        // POST: api/Products
        [HttpPost]
        [ProducesResponseType(typeof(Product), (int)HttpStatusCode.OK)]
        public IActionResult PostProduct([FromBody] ProductViewModel productViewModel)
        {
            if (!ModelState.IsValid)
            {
                return BadRequest(ModelState);
            }

            var product = productRepository.Add(productViewModel, User.GetId());

            if(product == null)
            {
                return BadRequest();
            }

            return CreatedAtRoute("GetProduct", new { id = product.Id }, product);
        }

        // DELETE: api/Products/5
        [HttpDelete("{id}")]
        [ProducesResponseType(typeof(Product), (int)HttpStatusCode.OK)]
        public IActionResult DeleteProduct(int id)
        {
            Product product = productRepository.FindById(id, x => x.User);

            if (product == null)
            {
                return NotFound();
            }

            var isRemoved = productRepository.Remove(product, User.GetId());

            if(!isRemoved)
            {
                return BadRequest();
            }

            return Ok(product);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                productRepository.Dispose();
            }
            base.Dispose(disposing);
        }

        private bool ProductExists(int id)
        {
            return productRepository.GetAll().Count(e => e.Id == id) > 0;
        }

        private IActionResult GetHttpResultForNullCheck(object obj)
        {
            if (obj == null)
            {
                return NotFound();
            }

            return Ok(obj);
        }
    }
}